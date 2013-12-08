// Copyright 2007-2009 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkevents.h>
#include <gdkmm/window.h>
#include <gtkmm/main.h>
#include <gtkmm/stock.h>
#include <gtkmm/stockid.h>

#include "frame.h"
#include "gui.hpp"
#include "mixer.hpp"
#include "mixer_window.hpp"

mixer_window::mixer_window(mixer & mixer)
    : mixer_(mixer),
      cut_button_("gtk-cut"),
      vu_meter_(-56, 0),
      wakeup_pipe_(O_NONBLOCK, O_NONBLOCK),
      next_source_id_(0)      
{
    add_events(Gdk::KEY_PRESS_MASK);

    cut_button_.set_use_stock();

    Glib::RefPtr<Glib::IOSource> pipe_io_source(
	Glib::IOSource::create(wakeup_pipe_.reader.get(), Glib::IO_IN));
    pipe_io_source->set_priority(Glib::PRIORITY_DEFAULT_IDLE);
    pipe_io_source->connect(sigc::mem_fun(this, &mixer_window::update));
    pipe_io_source->attach();

    set_mnemonic_modifier(Gdk::ModifierType(0));

    cut_button_.set_can_focus(false);
    cut_button_.set_sensitive(true);
    cut_button_.set_size_request(180, 80);
    cut_button_.signal_clicked().connect(sigc::mem_fun(mixer_, &mixer::cut));
    cut_button_.show();

    cut_sep_.show();

    vu_meter_.set_size_request(80, 300);
    vu_meter_.show();

    display_.show();

    selector_.set_border_width(gui_standard_spacing);
    selector_.set_accel_group(get_accel_group());
    selector_.signal_pri_video_selected().connect(
	sigc::mem_fun(*this, &mixer_window::set_pri_video_source));
    selector_.signal_audio_selected().connect(
	sigc::mem_fun(mixer_, &mixer::set_audio_source));
    selector_.show();

    vu_box_.set_spacing(gui_standard_spacing);
    vu_box_.pack_end(vu_meter_, Gtk::PACK_SHRINK);
    vu_box_.show();

    command_box_.set_spacing(gui_standard_spacing);
    command_box_.pack_start(cut_button_, Gtk::PACK_SHRINK);
    command_box_.pack_start(cut_sep_, Gtk::PACK_SHRINK);
    command_box_.pack_start(selector_, Gtk::PACK_SHRINK);
    command_box_.show();

    main_box_.set_spacing(gui_standard_spacing);
    main_box_.set_border_width(5);
    main_box_.pack_start(command_box_, Gtk::PACK_EXPAND_WIDGET);
    main_box_.pack_start(display_, Gtk::PACK_EXPAND_PADDING);
    main_box_.pack_start(vu_box_, Gtk::PACK_SHRINK);
    main_box_.show();
    add(main_box_);
}

mixer_window::~mixer_window()
{
}

void mixer_window::set_pri_video_source(mixer::source_id id)
{
    mixer_.set_video_source(id);
}

void mixer_window::put_frames(unsigned source_count,
			      const dv_frame_ptr * source_dv,
			      mixer::mix_settings mix_settings,
			      const dv_frame_ptr & mixed_dv,
			      const raw_frame_ptr & mixed_raw)
{
    {
	boost::mutex::scoped_lock lock(frame_mutex_);
	source_dv_.assign(source_dv, source_dv + source_count);
	mix_settings_ = mix_settings;
	mixed_dv_ = mixed_dv;
	mixed_raw_ = mixed_raw;
    }

    // Poke the event loop.
    static const char dummy[1] = {0};
    write(wakeup_pipe_.writer.get(), dummy, sizeof(dummy));
}

bool mixer_window::on_key_press_event(GdkEventKey *event)
{
    if (event->keyval == GDK_i && event->state & GDK_CONTROL_MASK) {
        selector_.toggle_audio_buttons();
    }
    return Window::on_key_press_event(event);
}

bool mixer_window::update(Glib::IOCondition) throw()
{
    // Empty the pipe (if frames have been dropped there's nothing we
    // can do about that now).
    static char dummy[4096];
    read(wakeup_pipe_.reader.get(), dummy, sizeof(dummy));

    try
    {
	dv_frame_ptr mixed_dv;
	std::vector<dv_frame_ptr> source_dv;
	raw_frame_ptr mixed_raw;

	{
	    boost::mutex::scoped_lock lock(frame_mutex_);
	    mixed_dv = mixed_dv_;
	    mixed_dv_.reset();
	    source_dv = source_dv_;
	    source_dv_.clear();
	    mixed_raw = mixed_raw_;
	    mixed_raw_.reset();
	}

	if (mixed_raw)
	    display_.put_frame(mixed_raw);
	else if (mixed_dv)
	    display_.put_frame(mixed_dv);
	if (mixed_dv)
	{
	    int levels[2];
	    dv_buffer_get_audio_levels(mixed_dv->buffer, levels);
	    vu_meter_.set_levels(levels);
	}

	selector_.set_source_count(source_dv.size());

	// Update the thumbnail displays of sources.  If a new mixed frame
	// arrives while we were doing this, return to the event loop.
	// (We want to handle the next mixed frame but we need to let it
	// handle other events as well.)  Use a rota for source updates so
	// even if we don't have time to run them all at full frame rate
	// they all get updated at roughly the same rate.

	for (std::size_t i = 0; i != source_dv.size(); ++i)
	{
	    if (next_source_id_ >= source_dv.size())
		next_source_id_ = 0;
	    mixer::source_id id = next_source_id_++;

	    if (source_dv[id])
	    {
		selector_.put_frame(id, source_dv[id]);

		boost::mutex::scoped_lock lock(frame_mutex_);
		if (mixed_dv_)
		    break;
	    }
	}
    }
    catch (std::exception & e)
    {
	std::cerr << "ERROR: Failed to update window: " << e.what() << "\n";
    }

    return true; // call again
}
