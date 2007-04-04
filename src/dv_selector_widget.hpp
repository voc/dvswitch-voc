// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_DV_SELECTOR_WIDGET_HPP
#define DVSWITCH_DV_SELECTOR_WIDGET_HPP

#include <vector>

#include <gtkmm/table.h>

#include "dv_display_widget.hpp"

class dv_selector_widget : public Gtk::Table
{
public:
    dv_selector_widget();

    void put_frame(mixer::source_id source_id,
		   const mixer::frame_ptr & source_frame);

private:
    std::vector<dv_thumb_display_widget *> thumbnails_;
};

#endif // !defined(DVSWITCH_DV_SELECTOR_WIDGET_HPP)
