// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <iostream>
#include <ostream>

#include <boost/bind.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/thread/thread.hpp>

#include <libdv/dv.h>

#include "frame.h"
#include "frame_timer.h"
#include "ring_buffer.hpp"
#include "mixer.hpp"

mixer::mixer()
    : monitor_(0),
      clock_thread_(0)
{
    source_queues_.reserve(5);
    sinks_.reserve(5);
}

mixer::~mixer()
{
    if (clock_thread_)
	stop_clock();
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
    source_queues_.push_back(frame_queue());
    return source_queues_.size() - 1;
}

void mixer::remove_source(source_id)
{
    // XXX We probably want to be able to reuse source slots.
}

void mixer::put_frame(source_id id, const frame_ptr & frame)
{
    bool was_full;

    {
	boost::mutex::scoped_lock lock(source_mutex_);

	frame_queue & queue = source_queues_.at(id);
	was_full = queue.full();

	if (!was_full)
	{
	    queue.push(frame);

	    // Start running once we have one half-full source queue.
	    if (!clock_thread_ && queue.size() == ring_buffer_size / 2)
	    {
		settings_.cut_before = false;
		settings_.video_source_id = id;
		start_clock();
	    }
	}
    }

    if (was_full)
	std::cerr << "ERROR: dropped frame from source " << id
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
    settings_.video_source_id = id;
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

void mixer::start_clock()
{
    // XXX This could take some time (e.g. it may allocate the thread
    // stack before returning). We should start the thread earlier and
    // only set a condition here.
    assert(!clock_thread_);
    clock_thread_ = new boost::thread(boost::bind(&mixer::run_clock, this));
}

void mixer::stop_clock()
{
    assert(clock_thread_);

    {
	// This is supposed to signal the clock thread to exit
	boost::mutex::scoped_lock lock(source_mutex_);
	source_queues_.clear();
    }

    // Wait for it to do so
    clock_thread_->join();
    delete clock_thread_;
    clock_thread_ = 0;
}

// Ensure the frame timer is initialised at startup
namespace
{
    struct timer_initialiser { timer_initialiser(); } timer_dummy;
    timer_initialiser::timer_initialiser()
    {
	init_frame_timer();
    }
}

void mixer::run_clock()
{
    dv_system_t last_frame_system = e_dv_system_none;
    std::vector<frame_ptr> source_frames;
    source_frames.reserve(5);
    frame_ptr mixed_frame;
    unsigned serial_num = 0;

    for (;;)
    {
	bool cut_before;

	// Select the mixer settings and source frame(s)
	// TODO: select frames from all sources for monitor
	{
	    boost::mutex::scoped_lock lock(source_mutex_);
	    if (source_queues_.size() == 0) // signal to exit
		break;
	    source_frames.resize(source_queues_.size());
	    cut_before = settings_.cut_before;
	    settings_.cut_before = false;
	    for (source_id id = 0; id != source_queues_.size(); ++id)
	    {
		if (!source_queues_[id].empty())
		{
		    source_frames[id] = source_queues_[id].front();
		    source_frames[id]->serial_num = serial_num;
		    if (id == settings_.video_source_id)
			mixed_frame = source_frames[id];
		    source_queues_[id].pop();
		}
	    }
	}

	assert(mixed_frame);
	++serial_num;

	// Sink the frame
	{
	    boost::mutex::scoped_lock lock(sink_mutex_);
	    for (sink_id id = 0; id != sinks_.size(); ++id)
		if (sinks_[id])
		{
		    if (cut_before)
			sinks_[id]->cut();
		    sinks_[id]->put_frame(mixed_frame);
		}
	}
	if (monitor_)
	    monitor_->put_frames(source_frames.size(), &source_frames[0],
				 mixed_frame);

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

	wait_frame_timer();
    }
}
