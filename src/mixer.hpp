// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_MIXER_HPP
#define DVSWITCH_MIXER_HPP

#include <cstddef>
#include <vector>

#include <tr1/memory>

#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include "ring_buffer.hpp"

namespace boost
{
    class thread;
}

class dv_frame;
class raw_frame;

class mixer
{
public:
    // Identifiers to distinguish mixer's sources and sinks
    typedef unsigned source_id, sink_id;
    static const unsigned invalid_id = -1;
    // Reference-counting pointers to frames
    typedef std::tr1::shared_ptr<dv_frame> dv_frame_ptr;
    typedef std::tr1::shared_ptr<raw_frame> raw_frame_ptr;

    // Settings for mixing/switching
    struct video_effect_settings;
    struct mix_settings
    {
	source_id video_source_id;
	std::tr1::shared_ptr<video_effect_settings> video_effect;
	source_id audio_source_id;
	bool cut_before;
    };

    // Interface to sinks
    struct sink
    {
	// Put a frame out.
	// The frame is shared with other sinks and must not be
	// modified.  It should be released as soon as possible.
	// This will be called at the appropriate frame rate even
	// if there are no new frames available.  The serial_num
	// member of the frame can be used to check whether the
	// frame is new.
	virtual void put_frame(const dv_frame_ptr &) = 0;
    };

    // Interface to monitor
    struct monitor
    {
	// Display or otherwise use frames.
	//
	// source_count is the number of sources assigned, though some
	// may no longer be registered.  source_dv points to an array,
	// length source_count, of pointers to the frames clocked
	// through from these sources.  Any or all of these pointers
	// may be null if the sources are not producing frames.
	// mix_settings is a copy of the settings used to select and
	// mix these source frames.  mixed_dv is a pointer to the
	// mixed frame that was sent to sinks.
	//
	// {video_{pri,sec}_source,mixed}_raw are pointers to decoded
	// versions of the primary and secondary video sources and the
	// mixed frame, if the mixer produced them in the course of
	// its work; any or all may be null.
	//
	// All DV frames may be shared and must not be modified.  Raw
	// frames may be modified by the monitor.  All references and
	// pointers passed to the function are invalid once it
	// returns; it must copy shared_ptrs to ensure that frames
	// remain valid.
	//
	// This is called in the context of the mixer thread and should
	// return quickly.
	virtual void put_frames(unsigned source_count,
				const dv_frame_ptr * source_dv,
				mix_settings,
				const dv_frame_ptr & mixed_dv) = 0;
    };

    mixer();
    ~mixer();

    // Interface for sources
    // Register and unregister sources
    source_id add_source();
    void remove_source(source_id);
    // Allocate a frame buffer.  This uses a memory pool and should be
    // fast.
    static dv_frame_ptr allocate_frame();
    // Add a new frame from the given source.  This should be called at
    // appropriate intervals to avoid the need to drop or duplicate
    // frames.
    void put_frame(source_id, const dv_frame_ptr &);

    // Interface for sinks
    // Register and unregister sinks
    sink_id add_sink(sink *);
    void remove_sink(sink_id);

    // Interface for monitors
    void set_monitor(monitor *);

    static std::tr1::shared_ptr<video_effect_settings>
    create_video_effect_pic_in_pic(source_id sec_source_id,
				   unsigned left, unsigned top,
				   unsigned right, unsigned bottom);
    static std::tr1::shared_ptr<video_effect_settings>
    null_video_effect()
    {
	return std::tr1::shared_ptr<video_effect_settings>();
    }

    // Mixer interface
    // Select the primary video source for output (this cancels any
    // video mixing effect)
    void set_video_source(source_id);
    // Set the video mixing effect (or cancel it, if the argument is
    // a null pointer)
    void set_video_effect(std::tr1::shared_ptr<video_effect_settings>);
    // Select the audio source for output
    void set_audio_source(source_id);
    // Make a cut in the output as soon as possible, where appropriate
    // for the sink
    void cut();

private:
    // Source data.  We want to allow a bit of leeway in the input
    // pipeline before we have to drop or repeat a frame.  At the
    // same time we don't want to add much to latency.  We try to
    // keep the queue half-full so there are 2 frame-times
    // (66-80 ms) of added latency here.
    static const std::size_t target_queue_len = 2;
    static const std::size_t full_queue_len = target_queue_len * 2;
    struct source_data
    {
	source_data() : is_live(true) {}
	bool is_live;
	ring_buffer<dv_frame_ptr, full_queue_len> frames;
    };

    struct mix_data
    {
	std::vector<dv_frame_ptr> source_frames;
	mix_settings settings;
    };

    enum run_state {
	run_state_wait,
	run_state_run,
	run_state_stop
    };

    void run_clock();   // clock thread function
    void run_mixer();   // mixer thread function

    boost::mutex source_mutex_; // controls access to the following
    mix_settings settings_;
    std::vector<source_data> sources_;
    run_state clock_state_;
    boost::condition clock_state_cond_;

    boost::thread clock_thread_;

    boost::mutex mixer_mutex_; // controls access to the following
    ring_buffer<mix_data, 3> mixer_queue_;
    run_state mixer_state_;
    boost::condition mixer_state_cond_;

    boost::thread mixer_thread_;

    boost::mutex sink_mutex_; // controls access to the following
    std::vector<sink *> sinks_;

    monitor * monitor_;
};

#endif // !defined(DVSWITCH_MIXER_HPP)
