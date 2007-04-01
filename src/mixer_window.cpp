// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <gtkmm/main.h>
#include <gtk/gtkhbox.h>

#include "mixer.hpp"
#include "mixer_window.hpp"

mixer_window::mixer_window(mixer & mixer)
    : mixer_(mixer),
      timeout_event_source_(Glib::TimeoutSource::create(20))
{
    mixer.add_sink(&display_);
    timeout_event_source_->connect(
	SigC::slot(display_, &dv_display_widget::try_update));
    timeout_event_source_->attach();
    signal_key_press_event().connect(
	SigC::slot(*this, &mixer_window::on_key_press));

    add(box_);
    box_.add(display_);
    display_.show();
    box_.add(selector_);
    selector_.show();
    box_.show();
}

mixer_window::~mixer_window()
{}

bool mixer_window::on_key_press(GdkEventKey * event)
{
    switch (event->keyval)
    {
	case 'c': // = cut
	    mixer_.cut();
	    return true;
	case 'q': // = quit
	    Gtk::Main::quit();
	    return true;
	default:
	    if (event->keyval >= '0' && event->keyval <= '9')
	    {
		// XXX We need to range-check this.
		mixer_.set_video_source(event->keyval - '0');
		return true;
	    }
	    return false;
    }
}
