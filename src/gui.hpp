#ifndef DVSWITCH_GUI_HPP
#define DVSWITCH_GUI_HPP

#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>
#include <gtkmm/icontheme.h>

const unsigned gui_standard_spacing = 6;

inline Glib::RefPtr<Gdk::Pixbuf> load_icon(const Glib::ustring & name, int size)
{
    return Gtk::IconTheme::get_default()->
	load_icon(name, size, Gtk::IconLookupFlags(0));
}

#endif
