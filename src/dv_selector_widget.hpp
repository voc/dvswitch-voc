// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_DV_SELECTOR_WIDGET_HPP
#define DVSWITCH_DV_SELECTOR_WIDGET_HPP

#include <gtkmm/table.h>

class dv_selector_widget : public Gtk::Table
{
public:
    void set_xv_port(uint32_t);
};

#endif // !defined(DVSWITCH_DV_SELECTOR_WIDGET_HPP)
