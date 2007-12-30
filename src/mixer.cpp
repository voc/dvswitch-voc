// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <cstddef>
#include <cstring>
#include <iostream>
#include <ostream>
#include <stdexcept>

#include <boost/bind.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/thread/thread.hpp>

#include <libdv/dv.h>

#include "frame.h"
#include "frame_timer.h"
#include "mixer.hpp"
#include "ring_buffer.hpp"
#include "video_effect.h"

mixer::mixer()
    : clock_state_(run_state_wait),
      clock_thread_(boost::bind(&mixer::run_clock, this)),
      mixer_state_(run_state_wait),
      mixer_thread_(boost::bind(&mixer::run_mixer, this)),
      monitor_(0)
{
    sources_.reserve(5);
    sinks_.reserve(5);
}

mixer::~mixer()
{
    {
	boost::mutex::scoped_lock lock(source_mutex_);
	clock_state_ = run_state_stop;
	clock_state_cond_.notify_one(); // in case it's still waiting
    }
    {
	boost::mutex::scoped_lock lock(mixer_mutex_);
	mixer_state_ = run_state_stop;
	mixer_state_cond_.notify_one();
    }

    clock_thread_.join();
    mixer_thread_.join();
}

// Memory pool for frame buffers.  This should make frame
// (de)allocation relatively cheap.

namespace
{
    boost::mutex dv_frame_pool_mutex; // controls access to the following
    boost::object_pool<dv_frame> dv_frame_pool(100);

    void free_dv_frame(dv_frame * frame)
    {
	boost::mutex::scoped_lock lock(dv_frame_pool_mutex);
	if (frame)
	    dv_frame_pool.free(frame);
    }

    boost::mutex raw_frame_pool_mutex; // controls access to the following
    boost::object_pool<raw_frame> raw_frame_pool(10);

    void free_raw_frame(raw_frame * frame)
    {
	boost::mutex::scoped_lock lock(raw_frame_pool_mutex);
	if (frame)
	    raw_frame_pool.free(frame);
    }

    mixer::raw_frame_ptr allocate_raw_frame()
    {
	boost::mutex::scoped_lock lock(raw_frame_pool_mutex);
	return mixer::raw_frame_ptr(raw_frame_pool.malloc(), free_raw_frame);
    }
}

mixer::dv_frame_ptr mixer::allocate_frame()
{
    boost::mutex::scoped_lock lock(dv_frame_pool_mutex);
    return dv_frame_ptr(dv_frame_pool.malloc(), free_dv_frame);
}

mixer::source_id mixer::add_source()
{
    boost::mutex::scoped_lock lock(source_mutex_);
    source_id id;
    for (id = 0; id != sources_.size(); ++id)
    {
	if (!sources_[id].is_live)
	{
	    sources_[id].is_live = true;
	    return id;
	}
    }
    sources_.resize(id + 1);
    return id;
}

void mixer::remove_source(source_id id)
{
    boost::mutex::scoped_lock lock(source_mutex_);
    sources_.at(id).is_live = false;
}

void mixer::put_frame(source_id id, const dv_frame_ptr & frame)
{
    bool was_full;
    bool should_notify_clock = false;

    {
	boost::mutex::scoped_lock lock(source_mutex_);

	source_data & source = sources_.at(id);
	was_full = source.frames.full();

	if (!was_full)
	{
	    frame->timestamp = frame_timer_get();
	    source.frames.push(frame);

	    // Start clock ticking once one source has reached the
	    // target queue length
	    if (clock_state_ == run_state_wait
		&& source.frames.size() == target_queue_len)
	    {
		settings_.video_source_id = id;
		settings_.audio_source_id = id;
		settings_.cut_before = false;
		clock_state_ = run_state_run;
		should_notify_clock = true; // after we unlock the mutex
	    }
	}
    }

    if (should_notify_clock)
	clock_state_cond_.notify_one();

    if (was_full)
	std::cerr << "WARN: Dropped frame from source " << 1 + id
		  << " due to full queue\n";
}

mixer::sink_id mixer::add_sink(sink * sink)
{
    boost::mutex::scoped_lock lock(sink_mutex_);
    // XXX We may want to be able to reuse sink slots.
    sinks_.push_back(sink);
    return sinks_.size() - 1;
}

void mixer::remove_sink(sink_id id)
{
    boost::mutex::scoped_lock lock(sink_mutex_);
    sinks_.at(id) = 0;
}

// Video effect settings.  In future this is likely to be an abstract
// base class but for now the only effect we will have is picture-in-
// picture.
struct mixer::video_effect_settings
{
    source_id sec_source_id;
    unsigned left, top;
    unsigned right, bottom;
};

std::tr1::shared_ptr<mixer::video_effect_settings>
mixer::create_video_effect_pic_in_pic(source_id sec_source_id,
				      unsigned left, unsigned top,
				      unsigned right, unsigned bottom)
{
    // XXX Need to validate parameters before they break anything
    std::tr1::shared_ptr<video_effect_settings> result(
	new video_effect_settings);
    result->sec_source_id = sec_source_id;
    result->left = left;
    result->top = top;
    result->right = right;
    result->bottom = bottom;
    return result;
}

void mixer::set_video_source(source_id id)
{
    boost::mutex::scoped_lock lock(source_mutex_);
    if (id < sources_.size())
    {
	settings_.video_source_id = id;
	settings_.video_effect.reset();
    }
    else
    {
	throw std::range_error("video source id out of range");
    }
}

void mixer::set_video_effect(
    std::tr1::shared_ptr<video_effect_settings> effect)
{
    boost::mutex::scoped_lock lock(source_mutex_);
    settings_.video_effect = effect;
}

void mixer::set_audio_source(source_id id)
{
    boost::mutex::scoped_lock lock(source_mutex_);
    if (id < sources_.size())
	settings_.audio_source_id = id;
    else
	throw std::range_error("audio source id out of range");
}

void mixer::set_monitor(monitor * monitor)
{
    assert(monitor && !monitor_);
    monitor_ = monitor;
}

void mixer::enable_record(bool flag)
{
    boost::mutex::scoped_lock lock(source_mutex_);
    settings_.do_record = flag;
}

void mixer::cut()
{
    boost::mutex::scoped_lock lock(source_mutex_);
    settings_.cut_before = true;
}

namespace
{
    // Ensure the frame timer is initialised at startup
    struct timer_initialiser { timer_initialiser(); } timer_dummy;
    timer_initialiser::timer_initialiser()
    {
	frame_timer_init();
    }

    void dub_audio(dv_frame & dest_frame, const dv_frame & source_frame)
    {
	// Copy AAUX blocks.  These are every 16th block in each DIF
	// sequence, starting from block 6.

	unsigned seq_count = (source_frame.system == e_dv_system_625_50
			      ? 12 : 10);
	assert(dest_frame.system == source_frame.system);

	for (unsigned seq_num = 0; seq_num != seq_count; ++seq_num)
	{
	    for (unsigned block_num = 0; block_num != 9; ++block_num)
	    {
		std::ptrdiff_t block_pos =
		    DIF_SEQUENCE_SIZE * seq_num
		    + DIF_BLOCK_SIZE * (6 + block_num * 16);
		std::memcpy(dest_frame.buffer + block_pos,
			    source_frame.buffer + block_pos,
			    DIF_BLOCK_SIZE);
	    }
	}
    }

    void silence_audio(dv_frame & dest_frame)
    {
	unsigned seq_count = (dest_frame.system == e_dv_system_625_50
			      ? 12 : 10);
	static const unsigned frequency = 48000;

	// Each audio block has a 3-byte block header, a 5-byte AAUX
	// header, and 72 bytes of samples.  Audio block 3 in each
	// sequence has an AS (audio source) header, audio block 4
	// has an ASC (audio source control) header, and the other
	// headers seem to be optional.
	static const uint8_t aaux_blank_header[DIF_AAUX_HEADER_SIZE] = {
	    0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	uint8_t aaux_as_header[DIF_AAUX_HEADER_SIZE] = {
	    // block type; 0x50 for AAUX source
	    0x50,
	    // bits 0-5: number of samples in frame minus minimum value
	    // bit 6: flag "should be 1"
	    // bit 7: flag for unlocked audio sampling
	    (dest_frame.system == e_dv_system_625_50
	     ? (frequency / 25 - 1896) : (frequency * 1001 / 30000 - 1580))
	    | (1 << 6) | (1 << 7),
	    // bits 0-3: audio mode
	    // bit 4: flag for independent channels
	    // bit 5: flag for "lumped" stereo (?)
	    // bits 6-7: number of audio channels per block minus 1
	    0,
	    // bits 0-4: system type; 0x0 for DV
	    // bit 5: frame rate; 0 for 29.97 fps, 1 for 25 fps
	    // bit 6: flag for multi-language audio
	    // bit 7: ?
	    (dest_frame.system == e_dv_system_625_50) << 5,
	    // bits 0-2: quantisation; 0 for 16-bit LPCM
	    // bits 3-5: sample frequency; 0 for 48 kHz
	    // bit 6: time constant of emphasis; must be 1
	    // bit 7: flag for no emphasis
	    (1 << 6) | (1 << 7)
	};
	static const uint8_t aaux_asc_header[DIF_AAUX_HEADER_SIZE] = {
	    // block type; 0x51 for AAUX source control
	    0x51,
	    // bits 0-1: emphasis flag and ?
	    // bits 2-3: compression level; 0 for once
	    // bits 4-5: input type; 1 for digital
	    // bits 6-7: copy generation management system; 0 for unrestricted
	    (1 << 4),
	    // bits 0-2: ?
	    // bits 3-5: recording mode; 1 for original (XXX should indicate dub)
	    // bit 6: recording end flag, inverted
	    // bit 7: recording start flag, inverted
	    (1 << 3) | (1 << 6) | (1 << 7),
	    // bits 0-6: speed; 0x20 seems to be normal
	    // bit 7: direction: 1 for forward
	    0x20 | (1 << 7),
	    // bits 0-6: genre; 0x7F seems to be unknown
	    // bit 7: reserved
	    0x7F
	};

	for (unsigned seq_num = 0; seq_num != seq_count; ++seq_num)
	{
	    for (unsigned block_num = 0; block_num != 9; ++block_num)
	    {
		std::ptrdiff_t block_pos =
		    DIF_SEQUENCE_SIZE * seq_num
		    + DIF_BLOCK_SIZE * (6 + block_num * 16);
		std::memcpy(dest_frame.buffer + block_pos
			    + DIF_BLOCK_HEADER_SIZE,
			    block_num == 3 ? aaux_as_header
			    : block_num == 4 ? aaux_asc_header
			    : aaux_blank_header,
			    DIF_AAUX_HEADER_SIZE);
		std::memset(dest_frame.buffer + block_pos
			    + DIF_BLOCK_HEADER_SIZE + DIF_AAUX_HEADER_SIZE,
			    0,
			    DIF_BLOCK_SIZE - DIF_BLOCK_HEADER_SIZE
			    - DIF_AAUX_HEADER_SIZE);
	    }
	}
    }
}

void mixer::run_clock()
{
    dv_system_t audio_source_system = e_dv_system_none;

    {
	boost::mutex::scoped_lock lock(source_mutex_);
	while (clock_state_ == run_state_wait)
	    clock_state_cond_.wait(lock);
    }

    // Interval to the next frame (in ns)
    unsigned frame_interval;
    // Weighted rolling average frame interval
    unsigned average_frame_interval;

    for (uint64_t tick_timestamp = frame_timer_get();
	 ;
	 tick_timestamp += frame_interval, frame_timer_wait(tick_timestamp))
    {
	mix_data m;

	// Select the mixer settings and source frame(s)
	{
	    boost::mutex::scoped_lock lock(source_mutex_);

	    if (clock_state_ == run_state_stop)
		break;

	    m.settings = settings_;
	    settings_.cut_before = false;

	    m.source_frames.resize(sources_.size());
	    for (source_id id = 0; id != sources_.size(); ++id)
	    {
		if (sources_[id].frames.empty())
		{
		    m.source_frames[id].reset();
		}
		else
		{
		    m.source_frames[id] = sources_[id].frames.front();
		    sources_[id].frames.pop();
		}
	    }
	}

	assert(m.settings.audio_source_id < m.source_frames.size()
	       && m.settings.video_source_id < m.source_frames.size());

	// Frame timer is based on the audio source.  Synchronisation
	// with the audio source matters more because audio
	// discontinuities are even more annoying than dropped or
	// repeated video frames.
	if (dv_frame * audio_source_frame =
	    m.source_frames[m.settings.audio_source_id].get())
	{
	    if (audio_source_system != audio_source_frame->system)
	    {
		audio_source_system = audio_source_frame->system;

		// Use standard frame timing initially.
		frame_interval = (audio_source_system == e_dv_system_625_50
				  ? frame_interval_ns_625_50
				  : frame_interval_ns_525_60);
		average_frame_interval = frame_interval;
	    }
	    else
	    {
		// The delay for this frame has a large effect on the
		// interval to the next frame because we want to
		// correct clock deviations quickly, but a much
		// smaller effect on the rolling average so that we
		// don't over-correct.  This has experimentally been
		// found to work well.
		static const unsigned next_average_weight = 3;
		static const unsigned next_delay_weight = 1;
		static const unsigned average_rolling_weight = 15;
		static const unsigned average_next_weight = 1;

		// Try to keep target_queue_len - 0.5 frame intervals
		// between delivery of source frames and mixing them.
		// The "obvious" way to feed the delay into the
		// frame_time is to divide it by target_queue_len-0.5.
		// But this is inverse to the effect we want it to
		// have: if the delay is long, we need to reduce,
		// not increase, frame_time.  So we calculate a kind
		// of inverse based on the amount of queue space
		// that should remain free.
		const uint64_t delay =
		    tick_timestamp > audio_source_frame->timestamp
		    ? tick_timestamp - audio_source_frame->timestamp
		    : 0;
		const unsigned free_queue_time =
		    full_queue_len * frame_interval > delay
		    ? full_queue_len * frame_interval - delay
		    : 0;
		frame_interval =
		    (average_frame_interval * next_average_weight
		     + (free_queue_time
			* 2 / (2 * (full_queue_len - target_queue_len) + 1)
			* next_delay_weight))
		    / (next_average_weight + next_delay_weight);

		average_frame_interval =
		    (average_frame_interval * average_rolling_weight
		     + frame_interval * average_next_weight)
		    / (average_rolling_weight + average_next_weight);
	    }
	}

	std::size_t free_len;

	{
	    boost::mutex::scoped_lock lock(mixer_mutex_);
	    free_len = mixer_queue_.capacity() - mixer_queue_.size();
	    if (free_len != 0)
	    {
		mixer_queue_.push(m); // really want to move m here
		mixer_state_ = run_state_run;
	    }
	}

	if (free_len != 0)
	{
	    mixer_state_cond_.notify_one();
	}
	else
	{
	    std::cerr << "ERROR: Dropped source frames due to"
		" full mixer queue\n";
	}
    }
}

namespace
{
    raw_frame_ref make_raw_frame_ref(const mixer::raw_frame_ptr & frame)
    {
	raw_frame_ref result = {
	    frame->buffer,
	    FRAME_BYTES_PER_PIXEL * FRAME_WIDTH,
	    (frame->system == e_dv_system_625_50)
	    ? FRAME_HEIGHT_625_50 : FRAME_HEIGHT_525_60
	};
	return result;
    }

    mixer::raw_frame_ptr decode_video_frame(
	dv_decoder_t * decoder, const mixer::dv_frame_ptr & dv_frame)
    {
	mixer::raw_frame_ptr result = allocate_raw_frame();
	result->system = dv_frame->system;

	uint8_t * pixels[1] = { result->buffer };
	int pitches[1] = { FRAME_BYTES_PER_PIXEL * FRAME_WIDTH };	
	dv_decode_full_frame(decoder,
			     dv_frame->buffer,
			     e_dv_color_yuv, pixels, pitches);
	return result;
    }
}

void mixer::run_mixer()
{
    dv_frame_ptr last_mixed_dv;
    unsigned serial_num = 0;
    const mix_data * m = 0;
    dv_decoder_t * decoder = dv_decoder_new(0, true, true);
    dv_set_quality(decoder, DV_QUALITY_BEST);
    dv_encoder_t * encoder = dv_encoder_new(false, true, true);

    for (;;)
    {
	// Get the next set of source frames and mix settings (or stop
	// if requested)
	{
	    boost::mutex::scoped_lock lock(mixer_mutex_);

	    if (m)
		mixer_queue_.pop();

	    while (mixer_state_ != run_state_stop && mixer_queue_.empty())
		mixer_state_cond_.wait(lock);
	    if (mixer_state_ == run_state_stop)
		break;

	    m = &mixer_queue_.front();
	}

	for (unsigned id = 0; id != m->source_frames.size(); ++id)
	    if (m->source_frames[id])
		m->source_frames[id]->serial_num = serial_num;

	const dv_frame_ptr & audio_source_dv =
	    m->source_frames[m->settings.audio_source_id];
	const dv_frame_ptr & video_pri_source_dv =
	    m->source_frames[m->settings.video_source_id];

	dv_frame_ptr mixed_dv;
	raw_frame_ptr video_pri_source_raw;
	raw_frame_ptr video_sec_source_raw;
	raw_frame_ptr mixed_raw;
    
	// If we have a single live source for both audio and video,
	// use the source frame unchanged.
	if (audio_source_dv
	    && m->settings.video_source_id == m->settings.audio_source_id
	    && !m->settings.video_effect)
	{
	    mixed_dv = audio_source_dv;
	}
	else
	{
	    if (video_pri_source_dv)
	    {
		if (m->settings.video_effect
		    && m->source_frames[m->settings.video_effect
					->sec_source_id])
		{
		    const dv_frame_ptr video_sec_source_dv =
			m->source_frames[m->settings.video_effect
					 ->sec_source_id];

		    // Decode primary (with metadata)
		    dv_parse_header(decoder, video_pri_source_dv->buffer);
		    tm pri_source_timestamp;
		    dv_get_recording_datetime_tm(decoder,
						 &pri_source_timestamp);
		    bool pri_source_is16x9 = dv_format_wide(decoder);
		    mixed_raw =
			decode_video_frame(decoder, video_pri_source_dv);

		    // Decode secondary
		    dv_parse_header(decoder, video_pri_source_dv->buffer);
		    video_sec_source_raw =
			decode_video_frame(decoder, video_sec_source_dv);

		    // Mix raw video
		    video_effect_pic_in_pic(
			make_raw_frame_ref(mixed_raw),
			make_raw_frame_ref(video_sec_source_raw),
			m->settings.video_effect->left,
			m->settings.video_effect->top,
			m->settings.video_effect->right,
			m->settings.video_effect->bottom);

		    // Encode mixed video
		    mixed_dv = allocate_frame();
		    mixed_dv->serial_num = serial_num;
		    mixed_dv->system = video_pri_source_dv->system;
		    mixed_dv->size = video_pri_source_dv->size;
		    // I LOVE THIS API
		    encoder->isPAL = mixed_dv->system == e_dv_system_625_50;
		    encoder->is16x9 = pri_source_is16x9;
		    time_t pri_source_time = mktime(&pri_source_timestamp);
		    dv_encode_metadata(mixed_dv->buffer,
				       encoder->isPAL,
				       encoder->is16x9,
				       &pri_source_time,
				       serial_num);
		    uint8_t * mixed_raw_pixels[1] = { mixed_raw->buffer };
		    dv_encode_full_frame(encoder,
					 mixed_raw_pixels,
					 e_dv_color_yuv,
					 mixed_dv->buffer);
		    // audio and timecode are handled below
		}
		else
		{
		    mixed_dv = video_pri_source_dv;
		}
	    }
	    else
	    {
		std::cerr << "WARN: Repeating frame due to empty queue"
		    " for source " << 1 + m->settings.video_source_id << "\n";

		// Make a copy of the last mixed frame so we can
		// replace the audio.  (We can't modify the last frame
		// because sinks may still be reading from it.)
		mixed_dv = allocate_frame();
		std::memcpy(mixed_dv.get(),
			    last_mixed_dv.get(),
			    offsetof(dv_frame, buffer) + last_mixed_dv->size);
		mixed_dv->serial_num = serial_num;
	    }

	    if (audio_source_dv && audio_source_dv->system == mixed_dv->system)
		dub_audio(*mixed_dv, *audio_source_dv);
	    else
		silence_audio(*mixed_dv);
	}

	dv_encode_timecode(mixed_dv->buffer,
			   mixed_dv->system == e_dv_system_625_50,
			   mixed_dv->serial_num);
	mixed_dv->do_record = m->settings.do_record;
	mixed_dv->cut_before = m->settings.cut_before;

	last_mixed_dv = mixed_dv;
	++serial_num;

	// Sink the frame
	{
	    boost::mutex::scoped_lock lock(sink_mutex_);
	    for (sink_id id = 0; id != sinks_.size(); ++id)
		if (sinks_[id])
		    sinks_[id]->put_frame(mixed_dv);
	}
	if (monitor_)
	    monitor_->put_frames(m->source_frames.size(), &m->source_frames[0],
				 m->settings, mixed_dv);
    }

    dv_encoder_free(encoder);
    dv_decoder_free(decoder);
}
