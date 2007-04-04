// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <gtkmm/label.h>

#include "dv_selector_widget.hpp"

namespace
{
    const unsigned thumbs_per_row = 4;
}

dv_selector_widget::dv_selector_widget()
{}

void dv_selector_widget::put_frame(mixer::source_id source_id,
				   const mixer::frame_ptr & source_frame)
{
    if (source_id >= thumbnails_.size())
    {
	resize(2 * ((source_id + thumbs_per_row) / thumbs_per_row),
	       thumbs_per_row);
	mixer::source_id first_new_source_id = thumbnails_.size();
	thumbnails_.resize(source_id + 1);

	for (mixer::source_id i = first_new_source_id; i <= source_id; ++i)
	{
	    dv_thumb_display_widget * thumb =
		manage(new dv_thumb_display_widget);
	    thumb->show();
	    attach(*thumb,
		   i % thumbs_per_row, i % thumbs_per_row + 1,
		   2 * (i / thumbs_per_row), 2 * (i / thumbs_per_row) + 1,
		   Gtk::FILL, Gtk::FILL,
		   6, 6);
	    thumbnails_[i] = thumb;

	    // XXX we'll be in trouble with > 9 sources
	    char label_text[2] = { '1' + i, 0 };
	    Gtk::Label * label = manage(new Gtk::Label(label_text));
	    label->show();
	    attach(*label,
		   i % thumbs_per_row, i % thumbs_per_row + 1,
		   2 * (i / thumbs_per_row) + 1, 2 * (i / thumbs_per_row) + 2,
		   Gtk::FILL, Gtk::FILL,
		   6, 6);
	}
    }

    thumbnails_[source_id]->put_frame(source_frame);
}
