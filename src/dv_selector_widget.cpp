// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <iostream>
#include <ostream>

#include <gtkmm/label.h>

#include "dv_selector_widget.hpp"

namespace
{
    const unsigned thumbs_per_row = 4;
    enum {
	row_display,
	row_labels,
	row_multiplier
    };
    enum {
	column_text_label,
	column_video_source_image,
	column_audio_source_image,
	column_multiplier
    };
    const unsigned padding_standard = 6;
}

dv_selector_widget::dv_selector_widget()
    : video_source_image_(SHAREDIR "/dvswitch/video-source.png"),
      last_video_source_id_(mixer::invalid_id),
      audio_source_image_(SHAREDIR "/dvswitch/audio-source.png"),
      last_audio_source_id_(mixer::invalid_id)
{
    video_source_image_.reference();
    video_source_image_.show();
    audio_source_image_.reference();
    audio_source_image_.show();
}

void dv_selector_widget::set_source_count(unsigned count)
{
    if (count > thumbnails_.size())
    {
	resize(((count + thumbs_per_row) / thumbs_per_row)
	       * row_multiplier,
	       thumbs_per_row * column_multiplier);
	mixer::source_id first_new_source_id = thumbnails_.size();

	try
	{
	    thumbnails_.resize(count);

	    for (mixer::source_id i = first_new_source_id; i != count; ++i)
	    {
		dv_thumb_display_widget * thumb =
		    manage(new dv_thumb_display_widget);
		thumb->show();
		attach(*thumb,
		       (i % thumbs_per_row) * column_multiplier,
		       (i % thumbs_per_row + 1) * column_multiplier,
		       (i / thumbs_per_row) * row_multiplier + row_display,
		       (i / thumbs_per_row) * row_multiplier + row_display + 1,
		       Gtk::FILL, Gtk::FILL,
		       padding_standard, padding_standard);
		thumbnails_[i] = thumb;

		// XXX we'll be in trouble with > 9 sources
		char label_text[2] = { '1' + i, 0 };
		Gtk::Label * label = manage(new Gtk::Label(label_text));
		label->show();
		attach(*label,
		       (i % thumbs_per_row) * column_multiplier
		       + column_text_label,
		       (i % thumbs_per_row) * column_multiplier
		       + column_text_label + 1,
		       (i / thumbs_per_row) * row_multiplier + row_labels,
		       (i / thumbs_per_row) * row_multiplier + row_labels + 1,
		       Gtk::FILL, Gtk::FILL,
		       padding_standard, padding_standard);
	    }
	}
	catch (std::exception & e)
	{
	    // Roll back size changes
	    thumbnails_.resize(first_new_source_id);
	    std::cerr << "ERROR: Failed to add source display: " << e.what()
		      << "\n";
	}
    }
}

void dv_selector_widget::set_video_source(mixer::source_id source_id)
{
    if (source_id != last_video_source_id_)
    {
	if (last_video_source_id_ != mixer::invalid_id)
	    remove(video_source_image_);
	attach(video_source_image_,
	       (source_id % thumbs_per_row) * column_multiplier
	       + column_video_source_image,
	       (source_id % thumbs_per_row) * column_multiplier
	       + column_video_source_image + 1,
	       (source_id / thumbs_per_row) * row_multiplier + row_labels,
	       (source_id / thumbs_per_row) * row_multiplier + row_labels + 1,
	       Gtk::FILL, Gtk::FILL,
	       padding_standard, padding_standard);
	last_video_source_id_ = source_id;
    }
}

void dv_selector_widget::set_audio_source(mixer::source_id source_id)
{
    if (source_id != last_audio_source_id_)
    {
	if (last_audio_source_id_ != mixer::invalid_id)
	    remove(audio_source_image_);
	attach(audio_source_image_,
	       (source_id % thumbs_per_row) * column_multiplier
	       + column_audio_source_image,
	       (source_id % thumbs_per_row) * column_multiplier
	       + column_audio_source_image + 1,
	       (source_id / thumbs_per_row) * row_multiplier + row_labels,
	       (source_id / thumbs_per_row) * row_multiplier + row_labels + 1,
	       Gtk::FILL, Gtk::FILL,
	       padding_standard, padding_standard);
	last_audio_source_id_ = source_id;
    }
}

void dv_selector_widget::put_frame(mixer::source_id source_id,
				   const mixer::frame_ptr & source_frame)
{
    if (source_id < thumbnails_.size())
	thumbnails_[source_id]->put_frame(source_frame);
}
