// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_MIXER_WINDOW_HPP
#define DVSWITCH_MIXER_WINDOW_HPP

#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <gtkmm/window.h>

#include "dv_display_widget.hpp"
#include "dv_selector_widget.hpp"

namespace Glib
{
    class TimeoutSource;
}

class mixer;

class mixer_window : public Gtk::Window
{
public:
    mixer_window(mixer & mixer);
    ~mixer_window();

private:
    bool on_key_press(GdkEventKey *);

    mixer & mixer_;

    Gtk::HBox box_;
    dv_display_widget display_;
    dv_selector_widget selector_;

    // XXX This is a hack to refresh the display at intervals.  We
    // should probably use a pipe for signalling new frames to avoid
    // waking up the main thread unnecessarily.
    Glib::RefPtr<Glib::TimeoutSource> timeout_event_source_;
};

#endif // !defined(DVSWITCH_MIXER_WINDOW_HPP)
