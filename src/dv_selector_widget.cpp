// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <iostream>
#include <ostream>

#include <gdk/gdkkeysyms.h>
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
	column_video_button,
	column_audio_button,
	column_multiplier
    };
    const unsigned padding_standard = 6;
}

dv_selector_widget::dv_selector_widget()
    : video_source_pixbuf_(
	  Gdk::Pixbuf::create_from_file(SHAREDIR "/dvswitch/video-source.png")),
      audio_source_pixbuf_(
	  Gdk::Pixbuf::create_from_file(SHAREDIR "/dvswitch/audio-source.png"))
{}

Gtk::RadioButton * dv_selector_widget::create_radio_button(
    Gtk::RadioButtonGroup & group,
    const Glib::RefPtr<Gdk::Pixbuf> & pixbuf)
{
    Gtk::Image * image = manage(new Gtk::Image(pixbuf));
    image->show();

    Gtk::RadioButton * button = manage(new Gtk::RadioButton(group));
    button->set_image(*image);
    button->set_mode(/*draw_indicator=*/false);

    return button;
}

void dv_selector_widget::set_accel_group(
    const Glib::RefPtr<Gtk::AccelGroup> & accel_group)
{
    assert(!accel_group_);
    accel_group_ = accel_group;
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
		unsigned column = (i % thumbs_per_row) * column_multiplier;
		unsigned row = (i / thumbs_per_row) * row_multiplier;

		dv_thumb_display_widget * thumb =
		    manage(new dv_thumb_display_widget);
		thumb->show();
		attach(*thumb,
		       column, column + column_multiplier,
		       row + row_display, row + row_display + 1,
		       Gtk::FILL, Gtk::FILL,
		       padding_standard, padding_standard);
		thumbnails_[i] = thumb;

		char label_text[4];
		snprintf(label_text, sizeof(label_text),
			 (i < 9) ? "_%u" : "%u", unsigned(1 + i));
		Gtk::Label * label =
		    manage(new Gtk::Label(label_text, true));
		label->show();
		attach(*label,
		       column + column_text_label,
		       column + column_text_label + 1,
		       row + row_labels, row + row_labels + 1,
		       Gtk::FILL, Gtk::FILL,
		       padding_standard, padding_standard);

		Gtk::RadioButton * video_button =
		    create_radio_button(video_button_group_,
					video_source_pixbuf_);
		video_button->signal_pressed().connect(
		    sigc::bind(
			sigc::mem_fun(*this,
				      &dv_selector_widget::on_video_selected),
			i));
		video_button->show();
		attach(*video_button,
		       column + column_video_button,
		       column + column_video_button + 1,
		       row + row_labels, row + row_labels + 1,
		       Gtk::FILL, Gtk::FILL,
		       padding_standard, padding_standard);

		Gtk::RadioButton * audio_button =
		    create_radio_button(audio_button_group_,
					audio_source_pixbuf_);
		audio_button->signal_pressed().connect(
		    sigc::bind(
			sigc::mem_fun(*this,
				      &dv_selector_widget::on_audio_selected),
			i));
		audio_button->show();
		attach(*audio_button,
		       column + column_audio_button,
		       column + column_audio_button + 1,
		       row + row_labels, row + row_labels + 1,
		       Gtk::FILL, Gtk::FILL,
		       padding_standard, padding_standard);

		if (i < 9)
		{
		    // Make the mnemonic on the label work.  Also make
		    // the numeric keypad and Alt-keys work.
		    label->set_mnemonic_widget(*video_button);
		    video_button->add_accelerator("activate",
						  accel_group_,
						  GDK_KP_1 + i,
						  Gdk::ModifierType(0),
						  Gtk::AccelFlags(0));
		    video_button->signal_activate().connect(
			sigc::bind(
			    sigc::mem_fun(
				*this,
				&dv_selector_widget::on_video_selected),
			    i));
		    audio_button->add_accelerator("activate",
						  accel_group_,
						  '1' + i,
						  Gdk::MOD1_MASK,
						  Gtk::AccelFlags(0));
		    audio_button->add_accelerator("activate",
						  accel_group_,
						  GDK_KP_1 + i,
						  Gdk::MOD1_MASK,
						  Gtk::AccelFlags(0));
		    audio_button->signal_activate().connect(
			sigc::bind(
			    sigc::mem_fun(
				*this,
				&dv_selector_widget::on_audio_selected),
			    i));
		}
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

void dv_selector_widget::put_frame(mixer::source_id source_id,
				   const mixer::dv_frame_ptr & source_frame)
{
    if (source_id < thumbnails_.size())
	thumbnails_[source_id]->put_frame(source_frame);
}

sigc::signal1<void, mixer::source_id> &
dv_selector_widget::signal_video_selected()
{
    return video_selected_signal_;
}

sigc::signal1<void, mixer::source_id> &
dv_selector_widget::signal_audio_selected()
{
    return audio_selected_signal_;
}

void dv_selector_widget::on_video_selected(mixer::source_id source_id)
{
    video_selected_signal_(source_id);
}

void dv_selector_widget::on_audio_selected(mixer::source_id source_id)
{
    audio_selected_signal_(source_id);
}

