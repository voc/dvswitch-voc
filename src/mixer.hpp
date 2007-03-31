// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_MIXER_HPP
#define DVSWITCH_MIXER_HPP

#include <cstddef>
#include <vector>

#include <tr1/memory>

#include <boost/pool/object_pool.hpp>
#include <boost/thread/mutex.hpp>

namespace boost
{
    class thread;
}

template<typename T, std::size_t N>
class ring_buffer;

class frame;
class sink;

class mixer
{
public:
    typedef unsigned source_id, sink_id;
    typedef std::tr1::shared_ptr<frame> frame_ptr;

    mixer();
    ~mixer();

    frame_ptr allocate_frame();

    // Source interface
    source_id add_source();
    void remove_source(source_id);
    void put_frame(source_id, const frame_ptr &);

    // Sink interface
    sink_id add_sink(sink *);
    void remove_sink(sink_id);

    // Mixer interface
    void set_video_source(source_id);
    void cut();

private:
    // XXX this is rather arbitrary
    static const unsigned ring_buffer_size = 10;

    typedef ring_buffer<frame_ptr, ring_buffer_size> frame_queue;

    // Settings for mixing/switching.  Rather simple at present. ;-)
    // If and when we do real mixing, these will need to be preserved
    // in a queue for the mixing thread(s) to apply before handing off
    // to the sinks.
    struct mix_settings
    {
	source_id video_source_id;
	bool cut_before;
    };

    void start_clock();
    void stop_clock();
    void run_clock();

    boost::mutex mixer_mutex_;
    mix_settings settings_;
    std::vector<frame_queue> source_queues_;
    std::vector<sink *> sinks_;
    boost::thread * clock_thread_;
};

struct sink
{
    virtual void put_frame(const mixer::frame_ptr &) = 0;
    virtual void cut() = 0;
};

#endif // !defined(DVSWITCH_MIXER_HPP)
