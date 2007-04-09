// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
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
#include "ring_buffer.hpp"
#include "mixer.hpp"

mixer::mixer()
    : clock_state_(clock_state_wait),
      clock_thread_(boost::bind(&mixer::run_clock, this)),
      monitor_(0)
{
    sources_.reserve(5);
    sinks_.reserve(5);
}

mixer::~mixer()
{
    {
	boost::mutex::scoped_lock lock(source_mutex_);
	clock_state_ = clock_state_stop;
	clock_state_cond_.notify_one(); // in case it's still waiting
    }

    clock_thread_.join();
}

// Memory pool for frame buffers.  This should make frame
// (de)allocation relatively cheap.

namespace
{
    boost::mutex frame_pool_mutex; // controls access to the following
    boost::object_pool<frame> frame_pool(100);

    void free_frame(frame * frame)
    {
	boost::mutex::scoped_lock lock(frame_pool_mutex);
	frame_pool.free(frame);
    }
}

mixer::frame_ptr mixer::allocate_frame()
{
    boost::mutex::scoped_lock lock(frame_pool_mutex);
    return frame_ptr(frame_pool.malloc(), free_frame);
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

void mixer::put_frame(source_id id, const frame_ptr & frame)
{
    bool was_full;
    bool should_notify_clock = false;

    {
	boost::mutex::scoped_lock lock(source_mutex_);

	source_data & source = sources_.at(id);
	was_full = source.frames.full();

	if (!was_full)
	{
	    source.frames.push(frame);

	    // Start clock ticking once we have one half-full source queue.
	    if (clock_state_ == clock_state_wait
		&& source.frames.size() == source.frames.capacity() / 2)
	    {
		settings_.video_source_id = id;
		settings_.audio_source_id = id;
		settings_.cut_before = false;
		clock_state_ = clock_state_run;
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

void mixer::set_video_source(source_id id)
{
    boost::mutex::scoped_lock lock(source_mutex_);
    if (id < sources_.size())
    settings_.video_source_id = id;
    else
	throw std::range_error("video source id out of range");
}

void mixer::set_monitor(monitor * monitor)
{
    assert(!monitor ^ !monitor_);
    monitor_ = monitor;
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
	init_frame_timer();
    }

    void dub_audio(frame & dest_frame, const frame & source_frame)
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

    void silence_audio(frame & dest_frame)
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
    dv_system_t last_frame_system = e_dv_system_none;
    std::vector<frame_ptr> source_frames;
    source_frames.reserve(5);
    frame_ptr last_mixed_frame;
    unsigned serial_num = 0;

    {
	boost::mutex::scoped_lock lock(source_mutex_);
	while (clock_state_ == clock_state_wait)
	    clock_state_cond_.wait(lock);
    }

    int frame_tick_count = 1;

    for (;;)
    {
	mix_settings settings;
	frame_ptr mixed_frame;

	// Select the mixer settings and source frame(s)
	{
	    boost::mutex::scoped_lock lock(source_mutex_);

	    if (clock_state_ == clock_state_stop)
		break;

	    settings = settings_;
	    settings_.cut_before = false;

	    source_frames.resize(sources_.size());
	    for (source_id id = 0; id != sources_.size(); ++id)
	    {
		if (sources_[id].frames.empty())
		{
		    source_frames[id].reset();
		}
		else
		{
		    source_frames[id] = sources_[id].frames.front();
		    source_frames[id]->serial_num = serial_num;
		    sources_[id].frames.pop();
		}
	    }
	}

	assert(settings.audio_source_id < source_frames.size()
	       && settings.video_source_id < source_frames.size());

	// If we have a single live source for both audio and video,
	// use the source frame unchanged.
	if (source_frames[settings.audio_source_id]
	    && settings.video_source_id == settings.audio_source_id)
	{
	    mixed_frame = source_frames[settings.audio_source_id];
	}
	else
	{
	    if (source_frames[settings.video_source_id])
	    {
		mixed_frame = source_frames[settings.video_source_id];
	    }
	    else
	    {
		std::cerr << "WARN: Repeating frame due to empty queue"
		    " for source " << 1 + settings.video_source_id << "\n";

		// Make a copy of the last mixed frame so we can
		// replace the audio.  (We can't modify the last frame
		// because sinks may still be reading from it.)
		mixed_frame = allocate_frame();
		std::memcpy(mixed_frame.get(),
			    last_mixed_frame.get(),
			    offsetof(frame, buffer) + last_mixed_frame->size);
		mixed_frame->serial_num = serial_num;
	    }

	    if (source_frames[settings.audio_source_id]
		&& (source_frames[settings.audio_source_id]->system
		    == mixed_frame->system))
	    {
		dub_audio(*mixed_frame,
			  *source_frames[settings.audio_source_id]);
	    }
	    else
	    {
		silence_audio(*mixed_frame);
	    }
	}

	mixed_frame->cut_before = settings.cut_before;

	last_mixed_frame = mixed_frame;
	++serial_num;

	// Sink the frame
	{
	    boost::mutex::scoped_lock lock(sink_mutex_);
	    for (sink_id id = 0; id != sinks_.size(); ++id)
		if (sinks_[id])
		    sinks_[id]->put_frame(mixed_frame);
	}
	if (monitor_)
	    monitor_->put_frames(source_frames.size(), &source_frames[0],
				 mixed_frame);

	if (--frame_tick_count == 0)
	{
	    // (Re)set the timer according to this frame's video system.
	    // TODO: Adjust timer interval dynamically to maintain synch with
	    // audio source.
	    if (mixed_frame->system != last_frame_system)
	    {
		last_frame_system = mixed_frame->system;
		set_frame_timer((mixed_frame->system == e_dv_system_525_60)
				? frame_time_ns_525_60
				: frame_time_ns_625_50);
	    }

	    frame_tick_count = wait_frame_timer();
	}
    }
}
