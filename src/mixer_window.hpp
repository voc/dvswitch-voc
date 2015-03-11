// Copyright 2007-2009 Ben Hutchings.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_MIXER_WINDOW_HPP
#define DVSWITCH_MIXER_WINDOW_HPP

#include <sys/types.h>

#include <boost/thread/mutex.hpp>

#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <gtkmm/imagemenuitem.h>
#include <gtkmm/separator.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/window.h>

#include "auto_pipe.hpp"
#include "dv_display_widget.hpp"
#include "dv_selector_widget.hpp"
#include "mixer.hpp"
#include "vu_meter.hpp"

namespace Glib
{
    class IOSource;
}

class mixer_window : public Gtk::Window, public mixer::monitor
{
public:
    explicit mixer_window(mixer & mixer, rectangle pip_area);
    ~mixer_window();

private:
    void cancel_effect();

    bool update(Glib::IOCondition) throw();

    void set_pri_video_source(mixer::source_id);
    void set_sec_video_source(mixer::source_id);
    void apply_pic_in_pic();

    virtual void put_frames(unsigned source_count,
			    const dv_frame_ptr * source_dv,
			    mixer::mix_settings,
			    const dv_frame_ptr & mixed_dv,
			    const raw_frame_ptr & mixed_raw);

    bool on_key_press_event(GdkEventKey* event);

    mixer & mixer_;

    Gtk::HBox main_box_;
    Gtk::VBox command_box_;
    Gtk::VBox vu_box_;
    Gtk::Button cut_button_;
    Gtk::ToggleButton pip_button_;
    Gtk::HSeparator cut_sep_;
    vu_meter vu_meter_;
    dv_full_display_widget display_;
    dv_selector_widget selector_;

    mixer::source_id sec_video_source_id_;
    mixer::source_id pri_video_source_id_;
    rectangle pip_area_;

    auto_pipe wakeup_pipe_;

    boost::mutex frame_mutex_; // controls access to the following
    std::vector<dv_frame_ptr> source_dv_;
    mixer::source_id next_source_id_;
    mixer::mix_settings mix_settings_;
    dv_frame_ptr mixed_dv_;
    raw_frame_ptr mixed_raw_;
};

#endif // !defined(DVSWITCH_MIXER_WINDOW_HPP)
