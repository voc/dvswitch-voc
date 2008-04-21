// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include <gdk/gdkkeysyms.h>
#include <gtkmm/main.h>
#include <gtkmm/stock.h>
#include <gtkmm/stockid.h>

#include "frame.h"
#include "gui.hpp"
#include "mixer.hpp"
#include "mixer_window.hpp"

// Window layout:
//
// +-------------------------------------------------------------------+
// | ╔═══════════════════════════════════════════════════════════════╗ |
// | ║+-----╥-------------------------------------------------------+║main_box_
// | ║|     ║                                                       |upper_box_
// | ║|     ║                                                       |║ |
// | ║|     ║                                                       |║ |
// | ║|     ║                                                       |║ |
// | ║|     ║                                                       |║ |
// | ║|     ║                                                       |║ |
// | ║|comm-║                                                       |║ |
// | ║|and_-║                       display_                        |║ |
// | ║|box_ ║                                                       |║ |
// | ║|     ║                                                       |║ |
// | ║|     ║                                                       |║ |
// | ║|     ║                                                       |║ |
// | ║|     ║                                                       |║ |
// | ║|     ║                                                       |║ |
// | ║|     ║                                                       |║ |
// | ║+-----╨-------------------------------------------------------+║ |
// | ╠═══════════════════════════════════════════════════════════════╣ |
// | ║                                                               ║ |
// | ║                                                               ║ |
// | ║                          selector_                            ║ |
// | ║                                                               ║ |
// | ║                                                               ║ |
// | ╚═══════════════════════════════════════════════════════════════╝ |
// +-------------------------------------------------------------------+

mixer_window::mixer_window(mixer & mixer)
    : mixer_(mixer),
      record_button_("gtk-media-record"),
      cut_button_("gtk-cut"),
      sec_video_source_id_(0),
      pip_active_(false),
      pip_pending_(false),
      wakeup_pipe_(O_NONBLOCK, O_NONBLOCK),
      next_source_id_(0)      
{
    record_button_.set_use_stock();
    cut_button_.set_use_stock();
	
    Glib::RefPtr<Glib::IOSource> pipe_io_source(
	Glib::IOSource::create(wakeup_pipe_.reader.get(), Glib::IO_IN));
    pipe_io_source->set_priority(Glib::PRIORITY_DEFAULT_IDLE);
    pipe_io_source->connect(sigc::mem_fun(this, &mixer_window::update));
    pipe_io_source->attach();

    set_border_width(gui_standard_spacing);
    set_mnemonic_modifier(Gdk::ModifierType(0));

    record_button_.set_mode(/*draw_indicator=*/false);
    record_button_.signal_toggled().connect(
	sigc::mem_fun(*this, &mixer_window::toggle_record));
    record_button_.show();

    cut_button_.set_sensitive(false);
    cut_button_.signal_clicked().connect(sigc::mem_fun(mixer_, &mixer::cut));
    cut_button_.show();

    display_.show();

    selector_.set_accel_group(get_accel_group());
    selector_.signal_pri_video_selected().connect(
	sigc::mem_fun(mixer_, &mixer::set_video_source));
    selector_.signal_sec_video_selected().connect(
	sigc::mem_fun(*this, &mixer_window::set_sec_video_source));
    selector_.signal_audio_selected().connect(
	sigc::mem_fun(mixer_, &mixer::set_audio_source));
    selector_.show();

    command_box_.set_spacing(gui_standard_spacing);
    command_box_.pack_start(record_button_, Gtk::PACK_SHRINK);
    command_box_.pack_start(cut_button_, Gtk::PACK_SHRINK);
    command_box_.show();

    upper_box_.set_spacing(gui_standard_spacing);
    upper_box_.add(command_box_);
    upper_box_.add(display_);
    upper_box_.show();

    main_box_.set_spacing(gui_standard_spacing);
    main_box_.add(upper_box_);
    main_box_.add(selector_);
    main_box_.show();
    add(main_box_);
}

bool mixer_window::on_key_press_event(GdkEventKey * event) throw()
{
    if (event->keyval == 'p')
    {
	if (pip_active_)
	{
	    pip_active_ = false;
	    mixer_.set_video_effect(mixer::null_video_effect());
	}
	else
	{
	    pip_pending_ = true;
	    display_.set_selection_enabled(true);
	}
	return true;
    }
 
    if (pip_pending_)
    {
	if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter)
	{
	    pip_active_ = true;
	    pip_pending_ = false;
	    display_.set_selection_enabled(false);

	    rectangle sel = display_.get_selection();
	    mixer_.set_video_effect(
		mixer_.create_video_effect_pic_in_pic(
		    sec_video_source_id_,
		    sel.left, sel.top, sel.right, sel.bottom));
	    return true;
	}
       
	if (event->keyval == GDK_Escape)
	{
	    pip_pending_ = false;
	    return true;
	}
    }

    if (event->keyval == 'q' && event->state & Gdk::CONTROL_MASK)
    {
	Gtk::Main::quit();
	return true;
    }

    return Gtk::Window::on_key_press_event(event);
}

void mixer_window::toggle_record() throw()
{
    bool flag = record_button_.get_active();
    mixer_.enable_record(flag);
    cut_button_.set_sensitive(flag);
}

void mixer_window::set_sec_video_source(mixer::source_id id)
{
    sec_video_source_id_ = id;
}

void mixer_window::update_video_effect()
{
    if (pip_active_)
    {
    }
    else
    {
    }
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
