// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_MIXER_WINDOW_HPP
#define DVSWITCH_MIXER_WINDOW_HPP

#include <sys/types.h>

#include <boost/thread/mutex.hpp>

#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <gtkmm/window.h>

#include "dv_display_widget.hpp"
#include "dv_selector_widget.hpp"
#include "mixer.hpp"

namespace Glib
{
    class TimeoutSource;
}

class mixer_window : public Gtk::Window, private mixer::monitor
{
public:
    explicit mixer_window(mixer & mixer);
    ~mixer_window();

private:
    bool on_key_press(GdkEventKey *);
    bool try_update();
    void grab_xv_port();
    void ungrab_xv_port();

    virtual void put_frames(unsigned source_count,
			    const mixer::frame_ptr * source_frames,
			    const mixer::frame_ptr & mixed_frame);

    mixer & mixer_;

    uint32_t xv_port_;

    Gtk::VBox box_;
    dv_full_display_widget display_;
    dv_selector_widget selector_;

    // XXX This is a hack to refresh the display at intervals.  We
    // should probably use a pipe for signalling new frames to avoid
    // waking up the main thread unnecessarily.
    Glib::RefPtr<Glib::TimeoutSource> timeout_event_source_;

    boost::mutex frame_mutex_; // controls access to the following
    std::vector<mixer::frame_ptr> source_frames_;
    mixer::frame_ptr mixed_frame_;
};

#endif // !defined(DVSWITCH_MIXER_WINDOW_HPP)
