// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include <gdk/gdkkeysyms.h>
#include <gtkmm/main.h>

#include "mixer.hpp"
#include "mixer_window.hpp"

mixer_window::mixer_window(mixer & mixer)
    : mixer_(mixer),
      next_source_id_(0)
{
    if (pipe(pipe_ends_) != 0)
	throw std::runtime_error(
	    std::string("pipe: ").append(std::strerror(errno)));
    fcntl(pipe_ends_[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_ends_[1], F_SETFL, O_NONBLOCK);
    pipe_io_source_ = Glib::IOSource::create(pipe_ends_[0], Glib::IO_IN);
    pipe_io_source_->set_priority(Glib::PRIORITY_DEFAULT_IDLE);
    pipe_io_source_->connect(SigC::slot(*this, &mixer_window::update));
    pipe_io_source_->attach();

    mixer_.set_monitor(this);

    add(box_);
    box_.add(display_);
    display_.show();
    box_.add(selector_);
    selector_.show();
    box_.show();
}

mixer_window::~mixer_window()
{
    // pipe_ends_[0] will be closed by pipe_io_source_ (I think)
    close(pipe_ends_[1]);
}

bool mixer_window::on_key_press_event(GdkEventKey * event)
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

    if (event->keyval >= '1' && event->keyval <= '9'
	|| event->keyval >= GDK_KP_1 && event->keyval <= GDK_KP_9)
    {
	mixer::source_id id;
	if (event->keyval >= '1' && event->keyval <= '9')
	    id = event->keyval - '1';
	else
	    id = event->keyval - GDK_KP_1;
	try
	{
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
			      const mixer::frame_ptr & mixed_frame)
{
    {
	boost::mutex::scoped_lock lock(frame_mutex_);
	source_frames_.assign(source_frames, source_frames + source_count);
	mixed_frame_ = mixed_frame;
    }

    // Poke the event loop.
    static const char dummy[1] = {0};
    write(pipe_ends_[1], dummy, sizeof(dummy));
}

bool mixer_window::update(Glib::IOCondition)
{
    // Empty the pipe (if frames have been dropped there's nothing we
    // can do about that now).
    static char dummy[4096];
    read(pipe_ends_[0], dummy, sizeof(dummy));

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

    return true; // call again
}
