// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <iostream>
#include <ostream>

#include <gtkmm/main.h>

#include "mixer.hpp"
#include "mixer_window.hpp"

// X headers come last due to egregious macro pollution.
#include "gtk_x_utils.hpp"
#include <X11/extensions/Xvlib.h>

mixer_window::mixer_window(mixer & mixer)
    : mixer_(mixer),
      xv_port_(XvPortID(-1)),
      display_(dv_display_widget::display_type_full),
      timeout_event_source_(Glib::TimeoutSource::create(20))
{
    mixer_.set_monitor(this);

    timeout_event_source_->connect(
	SigC::slot(*this, &mixer_window::try_update));
    timeout_event_source_->attach();

    signal_key_press_event().connect(
	SigC::slot(*this, &mixer_window::on_key_press));
    signal_show().connect(SigC::slot(*this, &mixer_window::grab_xv_port));
    signal_hide().connect(SigC::slot(*this, &mixer_window::ungrab_xv_port));

    add(box_);
    box_.add(display_);
    display_.show();
    box_.add(selector_);
    selector_.show();
    box_.show();
}

mixer_window::~mixer_window()
{
    ungrab_xv_port();
}

void mixer_window::grab_xv_port()
{
    Display * display = get_x_display(*this);
    unsigned adaptor_count;
    XvAdaptorInfo * adaptor_info;

    if (XvQueryAdaptors(display, get_x_window(*this),
			&adaptor_count, &adaptor_info) != Success)
    {
	std::cerr << "ERROR: XvQueryAdaptors() failed\n";
	return;
    }

    // Search for a suitable adaptor.
    const int target_format_id = dv_display_widget::pixel_format_id;
    unsigned i;
    for (i = 0; i != adaptor_count; ++i)
    {
	if (!(adaptor_info[i].type & XvImageMask))
	    continue;
	int format_count;
	XvImageFormatValues * format_info =
	    XvListImageFormats(display, adaptor_info[i].base_id,
			       &format_count);
	if (!format_info)
	    continue;
	for (int j = 0; j != format_count; ++j)
	    if (format_info[j].id == target_format_id)
		goto end_adaptor_loop;
    }
end_adaptor_loop:
    if (i == adaptor_count)
    {
	std::cerr << "ERROR: No Xv adaptor for this display supports "
		  << char(target_format_id >> 24)
		  << char((target_format_id >> 16) & 0xFF)
		  << char((target_format_id >> 8) & 0xFF)
		  << char(target_format_id & 0xFF)
		  << " format\n";
    }
    else
    {
	// Try to allocate a port.
	unsigned j;
	for (j = 0; j != adaptor_info[i].num_ports; ++j)
	{
	    XvPortID port = adaptor_info[i].base_id + i;
	    if (XvGrabPort(display, port, CurrentTime) == Success)
	    {
		xv_port_ = port;
		display_.set_xv_port(port);
		selector_.set_xv_port(port);
		break;
	    }
	}
	if (j == adaptor_info[i].num_ports)
	    std::cerr << "ERROR: Could not grab an Xv port\n";
    }

    XvFreeAdaptorInfo(adaptor_info);
}

void mixer_window::ungrab_xv_port()
{
    if (xv_port_ != XvPortID(-1))
    {
	display_.set_xv_port(-1);
	selector_.set_xv_port(-1);
	XvUngrabPort(get_x_display(*this), xv_port_, CurrentTime);
	xv_port_ = -1;
    }
}

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
    {
	boost::mutex::scoped_lock lock(frame_mutex_);
	source_frames_.clear();
	mixed_frame = mixed_frame_;
	mixed_frame_.reset();
    }

    if (mixed_frame)
	display_.put_frame(mixed_frame);

    return true; // call again
}
