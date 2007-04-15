// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include <gdk/gdkkeysyms.h>
#include <gtkmm/main.h>

#include "mixer.hpp"
#include "mixer_window.hpp"

mixer_window::mixer_window(mixer & mixer)
    : mixer_(mixer),
      wakeup_pipe_(O_NONBLOCK, O_NONBLOCK),
      next_source_id_(0)
{
    Glib::RefPtr<Glib::IOSource> pipe_io_source(
	Glib::IOSource::create(wakeup_pipe_.reader.get(), Glib::IO_IN));
    pipe_io_source->set_priority(Glib::PRIORITY_DEFAULT_IDLE);
    pipe_io_source->connect(SigC::slot(*this, &mixer_window::update));
    pipe_io_source->attach();

    add(box_);
    box_.add(display_);
    display_.show();
    box_.add(selector_);
    selector_.show();
    box_.show();
}

bool mixer_window::on_key_press_event(GdkEventKey * event) throw()
{
    if (event->keyval == 'c')
    {
	mixer_.cut();
	return true;
    }

    if (event->keyval == 'q' && event->state & Gdk::CONTROL_MASK)
    {
	Gtk::Main::quit();
	return true;
    }

    if ((event->keyval >= '1' && event->keyval <= '9'
	 || event->keyval >= GDK_KP_1 && event->keyval <= GDK_KP_9)
	&& !(event->state & (Gdk::SHIFT_MASK | Gdk::CONTROL_MASK)))
    {
	mixer::source_id id;
	if (event->keyval >= '1' && event->keyval <= '9')
	    id = event->keyval - '1';
	else
	    id = event->keyval - GDK_KP_1;
	try
	{
	    if (event->state & Gdk::MOD1_MASK) // Mod1 normally means Alt
		mixer_.set_audio_source(id);
	    else
		mixer_.set_video_source(id);
	}
	catch (std::range_error &)
	{
	    // never mind
	}
	return true;
    }

    return false;
}

void mixer_window::put_frames(unsigned source_count,
			      const mixer::frame_ptr * source_frames,
			      mixer::mix_settings mix_settings,
			      const mixer::frame_ptr & mixed_frame)
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

	selector_.set_source_count(source_frames.size());
	selector_.set_video_source(mix_settings_.video_source_id);
	selector_.set_audio_source(mix_settings_.audio_source_id);

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
