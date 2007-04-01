// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <iostream>
#include <ostream>

#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

#include <libdv/dv.h>

#include "frame.h"
#include "frame_timer.h"
#include "ring_buffer.hpp"
#include "mixer.hpp"

mixer::mixer()
    : clock_thread_(0)
{
    source_queues_.reserve(5);
    sinks_.reserve(5);
}

mixer::~mixer()
{
    if (clock_thread_)
	stop_clock();
}

namespace
{
    boost::mutex frame_pool_mutex;
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

void mixer::cut()
{
    boost::mutex::scoped_lock lock(source_mutex_);
    settings_.cut_before = true;
}

void mixer::start_clock()
{
    assert(!clock_thread_);
    clock_thread_ = new boost::thread(boost::bind(&mixer::run_clock, this));
}

void mixer::stop_clock()
{
    assert(clock_thread_);

    {
	boost::mutex::scoped_lock lock(source_mutex_);
	source_queues_.clear();
    }

    clock_thread_->join();
    delete clock_thread_;
    clock_thread_ = 0;
}

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
    frame_ptr frame;

    for (;;)
    {
	bool cut_before;

	{
	    boost::mutex::scoped_lock lock(source_mutex_);
	    cut_before = settings_.cut_before;
	    settings_.cut_before = false;
	    for (source_id id = 0; id != source_queues_.size(); ++id)
	    {
		if (!source_queues_[id].empty())
		{
		    if (id == settings_.video_source_id)
			// TODO: Mix in audio if audio source is different.
			frame = source_queues_[id].front();
		    source_queues_[id].pop();
		}
	    }
	}

	assert(frame);

	{
	    boost::mutex::scoped_lock lock(sink_mutex_);
	    for (sink_id id = 0; id != sinks_.size(); ++id)
		if (sinks_[id])
		{
		    if (cut_before)
			sinks_[id]->cut();
		    sinks_[id]->put_frame(frame);
		}
	}

	// (Re)set the timer according to this frame's video system.
	// TODO: Adjust timer interval dynamically to maintain synch with
	// audio source.
	if (frame->system != last_frame_system)
	{
	    last_frame_system = frame->system;
	    set_frame_timer((frame->system == e_dv_system_525_60)
			    ? frame_time_ns_525_60
			    : frame_time_ns_625_50);
	}

	wait_frame_timer();
    }
}
