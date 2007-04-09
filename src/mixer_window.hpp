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
    class IOSource;
}

class mixer_window : public Gtk::Window, public mixer::monitor
{
public:
    explicit mixer_window(mixer & mixer);
    ~mixer_window();

private:
    virtual bool on_key_press_event(GdkEventKey *);
    bool update(Glib::IOCondition);

    virtual void put_frames(unsigned source_count,
			    const mixer::frame_ptr * source_frames,
			    const mixer::frame_ptr & mixed_frame);

    mixer & mixer_;

    Gtk::VBox box_;
    dv_full_display_widget display_;
    dv_selector_widget selector_;

    int pipe_ends_[2];
    Glib::RefPtr<Glib::IOSource> pipe_io_source_;

    boost::mutex frame_mutex_; // controls access to the following
    std::vector<mixer::frame_ptr> source_frames_;
    mixer::source_id next_source_id_;
    mixer::frame_ptr mixed_frame_;
};

#endif // !defined(DVSWITCH_MIXER_WINDOW_HPP)
