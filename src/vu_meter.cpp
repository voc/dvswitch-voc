// Copyright 2009 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <limits>

#include "vu_meter.hpp"

vu_meter::vu_meter(int minimum, int maximum)
    : minimum_(minimum),
      maximum_(maximum),
      level_(std::numeric_limits<int>::min())
{
    Gdk::Color black;
    black.set_grey(0);
    modify_bg(Gtk::STATE_NORMAL, black);

    set_size_request(16, 32);
}

void vu_meter::set_level(int level)
{
    level_ = level;
    queue_draw();
}

bool vu_meter::on_expose_event(GdkEventExpose *) throw()
{
    int width = get_width(), height = get_height();
    Glib::RefPtr<Gdk::Drawable> drawable;
    int base_x, base_y;
    get_window()->get_internal_paint_info(drawable, base_x, base_y);
    drawable->reference(); // get_internal_paint_info() doesn't do this!

    if (Glib::RefPtr<Gdk::GC> gc = Gdk::GC::create(drawable))
    {
	// Draw segments of height 4 with 2 pixel (black) dividers.
	// The segments fade from green (min) to red (max).

	static const int border_thick = 2;
	static const int seg_height = 4, seg_spacing = seg_height + border_thick;

	int seg_count = (height - border_thick) / seg_spacing;

	if (seg_count > 0 && level_ >= minimum_)
	{
	    Gdk::Color colour;
	    int seg_lit_count = 1 + (((seg_count - 1) * (level_ - minimum_)
				      + ((maximum_ - minimum_) / 2))
				     / (maximum_ - minimum_));
	    for (int seg = 0; seg < seg_lit_count; ++seg)
	    {
		colour.set_rgb(65535 * seg / seg_count,
			       65535 * (seg_count - seg) / seg_count,
			       0);
		gc->set_rgb_fg_color(colour);
		drawable->draw_rectangle(gc, true,
					 base_x + border_thick,
					 base_y + (seg_count - seg) * seg_spacing
					 - seg_height,
					 width - 2 * border_thick, seg_height);
	    }
	}
    }

    return true;
}
