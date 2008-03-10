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

#include "gui.hpp"
#include "mixer.hpp"
#include "mixer_window.hpp"

mixer_window::mixer_window(mixer & mixer)
    : mixer_(mixer),
      record_button_("gtk-media-record"),
      cut_button_("gtk-cut"),
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

    record_button_.set_active(true);
    record_button_.set_mode(/*draw_indicator=*/false);
    record_button_.signal_toggled().connect(
	sigc::mem_fun(*this, &mixer_window::toggle_record));
    record_button_.show();

    cut_button_.signal_clicked().connect(sigc::mem_fun(mixer_, &mixer::cut));
    cut_button_.show();

    display_.show();

    selector_.set_accel_group(get_accel_group());
    selector_.signal_video_selected().connect(
	sigc::mem_fun(mixer_, &mixer::set_video_source));
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

void mixer_window::put_frames(unsigned source_count,
			      const mixer::dv_frame_ptr * source_frames,
			      mixer::mix_settings mix_settings,
			      const mixer::dv_frame_ptr & mixed_frame)
{
    {
	boost::mutex::scoped_lock lock(frame_mutex_);
	source_frames_.assign(source_frames, source_frames + source_count);
	mix_settings_ = mix_settings;
	mixed_frame_ = mixed_frame;
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
	mixer::dv_frame_ptr mixed_frame;
	std::vector<mixer::dv_frame_ptr> source_frames;
	{
	    boost::mutex::scoped_lock lock(frame_mutex_);
	    mixed_frame = mixed_frame_;
	    mixed_frame_.reset();
	    source_frames = source_frames_;
	    source_frames_.clear();
	}

	if (mixed_frame)
	    display_.put_frame(mixed_frame);

	selector_.set_source_count(source_frames.size());

	// Update the thumbnail displays of sources.  If a new mixed frame
	// arrives while we were doing this, return to the event loop.
	// (We want to handle the next mixed frame but we need to let it
	// handle other events as well.)  Use a rota for source updates so
	// even if we don't have time to run them all at full frame rate
	// they all get updated at roughly the same rate.

	for (std::size_t i = 0; i != source_frames.size(); ++i)
	{
	    if (next_source_id_ >= source_frames.size())
		next_source_id_ = 0;
	    mixer::source_id id = next_source_id_++;

	    if (source_frames[id])
	    {
		selector_.put_frame(id, source_frames[id]);

		boost::mutex::scoped_lock lock(frame_mutex_);
		if (mixed_frame_)
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
