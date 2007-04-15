// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_DV_SELECTOR_WIDGET_HPP
#define DVSWITCH_DV_SELECTOR_WIDGET_HPP

#include <vector>

#include <gtkmm/image.h>
#include <gtkmm/table.h>

#include "dv_display_widget.hpp"

class dv_selector_widget : public Gtk::Table
{
public:
    dv_selector_widget();

    void set_source_count(unsigned);
    void set_video_source(mixer::source_id);
    void set_audio_source(mixer::source_id);
    void put_frame(mixer::source_id source_id,
		   const mixer::frame_ptr & source_frame);

private:
    Gtk::Image video_source_image_;
    mixer::source_id last_video_source_id_;
    Gtk::Image audio_source_image_;
    mixer::source_id last_audio_source_id_;
    std::vector<dv_thumb_display_widget *> thumbnails_;
};

#endif // !defined(DVSWITCH_DV_SELECTOR_WIDGET_HPP)
