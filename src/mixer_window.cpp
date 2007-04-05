// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <gtkmm/main.h>

#include "mixer.hpp"
#include "mixer_window.hpp"

// X headers come last due to egregious macro pollution.
#include "gtk_x_utils.hpp"
#include <X11/extensions/Xvlib.h>

mixer_window::mixer_window(mixer & mixer)
    : mixer_(mixer),
      timeout_event_source_(Glib::TimeoutSource::create(20))
{
    mixer_.set_monitor(this);

    timeout_event_source_->connect(
	SigC::slot(*this, &mixer_window::try_update));
    timeout_event_source_->attach();

    add(box_);
    box_.add(display_);
    display_.show();
    box_.add(selector_);
    selector_.show();
    box_.show();
}

mixer_window::~mixer_window()
{}

bool mixer_window::on_key_press_event(GdkEventKey * event)
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
	    if (event->keyval >= '1' && event->keyval <= '9')
	    {
		// XXX We need to range-check this.
		mixer_.set_video_source(event->keyval - '1');
		return true;
	    }
	    return false;
    }
}

void mixer_window::put_frames(unsigned source_count,
			      const mixer::frame_ptr * source_frames,
			      const mixer::frame_ptr & mixed_frame)
{
    boost::mutex::scoped_lock lock(frame_mutex_);
    source_frames_.assign(source_frames, source_frames + source_count);
    mixed_frame_ = mixed_frame;
}

bool mixer_window::try_update()
{
    mixer::frame_ptr mixed_frame;
    std::vector<mixer::frame_ptr> source_frames;
    {
	boost::mutex::scoped_lock lock(frame_mutex_);
	mixed_frame = mixed_frame_;
	mixed_frame_.reset();
	source_frames = source_frames_;
	source_frames_.clear();
    }

    if (mixed_frame)
	display_.put_frame(mixed_frame);
    for (mixer::source_id i = 0; i != source_frames.size(); ++i)
	if (source_frames[i])
	    selector_.put_frame(i, source_frames[i]);
    // TODO: Update one thumbnail at a time and restart if new frames have
    // arrived.  Not sure how this will work with the interval timer; it
    // may be dependent on switching to use of a pipe.

    return true; // call again
}
