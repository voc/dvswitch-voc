// Copyright 2007-2008 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <iostream>
#include <ostream>

#include <gdk/gdkkeysyms.h>
#include <gtkmm/label.h>
#include <gtkmm/separator.h>

#include "dv_selector_widget.hpp"
#include "gui.hpp"

extern bool expert_mode;

namespace
{
    enum {
	column_labels,
	column_display,
	column_multiplier
    };
    enum {
	row_text_label,
	row_pri_video_button,
	row_sec_video_button,
	row_audio_button,
	row_separator,
	row_multiplier
    };
}

dv_selector_widget::dv_selector_widget()
    : pri_video_source_pixbuf_(
	  Gdk::Pixbuf::create_from_file(SHAREDIR
					"/dvswitch-voc/pri-video-source.png")),

      sec_video_source_pixbuf_(
	  Gdk::Pixbuf::create_from_file(SHAREDIR
					"/dvswitch-voc/sec-video-source.png")),

      audio_source_pixbuf_(
	  Gdk::Pixbuf::create_from_file(SHAREDIR
	  				"/dvswitch-voc/audio-source.png"))
{
    set_col_spacings(gui_standard_spacing);
    set_row_spacings(gui_standard_spacing);
}

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
	resize(count, 1);
	mixer::source_id first_new_source_id = thumbnails_.size();

	try
	{
	    thumbnails_.resize(count);

	    for (mixer::source_id i = first_new_source_id; i != count; ++i)
	    {
		unsigned column = 1;
		unsigned row = i * row_multiplier;

		if (0)//(i != 0 && i != count - 1)
		{
		    Gtk::HSeparator * sep = manage(new Gtk::HSeparator);
		    sep->show();
		    attach(*sep,
			   column - 1, column,
			   row, row + row_multiplier,
			   Gtk::FILL, Gtk::FILL,
			   0, 0);
		}

		dv_thumb_display_widget * thumb =
		    manage(new dv_thumb_display_widget);
		thumb->show();
		attach(*thumb,
		       column + column_display, column + column_display + 1,
		       row, row + row_multiplier,
		       Gtk::FILL, Gtk::FILL,
		       0, 0);
		thumbnails_[i] = thumb;

		char label_text[4];
		snprintf(label_text, sizeof(label_text),
			 "%u", unsigned(1 + i));
		Gtk::Label * label =
		    manage(new Gtk::Label(label_text));
		label->show();
		attach(*label,
		       column + column_labels, column + column_labels + 1,
		       row + row_text_label, row + row_text_label + 1,
		       Gtk::FILL, Gtk::FILL,
		       0, 0);

		Gtk::RadioButton * pri_video_button =
		    create_radio_button(pri_video_button_group_,
					pri_video_source_pixbuf_);
		pri_video_button->signal_pressed().connect(
		    sigc::bind(
			sigc::mem_fun(*this,
				      &dv_selector_widget::on_pri_video_selected),
			i));
		pri_video_button->show();
		attach(*pri_video_button,
		       column + column_labels, column + column_labels + 1,
		       row + row_pri_video_button,
		       row + row_pri_video_button + 1,
		       Gtk::FILL, Gtk::FILL,
		       0, 0);

		Gtk::RadioButton * sec_video_button =
		    create_radio_button(sec_video_button_group_,
					sec_video_source_pixbuf_);
		sec_video_button->signal_pressed().connect(
		    sigc::bind(
			sigc::mem_fun(*this,
				      &dv_selector_widget::on_sec_video_selected),
			i));
		sec_video_button->show();
		attach(*sec_video_button,
		       column + column_labels, column + column_labels + 1,
		       row + row_sec_video_button,
		       row + row_sec_video_button + 1,
		       Gtk::FILL, Gtk::FILL,
		       0, 0);

		Gtk::RadioButton * audio_button =
		    create_radio_button(audio_button_group_,
					audio_source_pixbuf_);
		audio_button_list_.push_back(audio_button);
		// disable by default, if not in expert mode
		audio_button->set_sensitive(expert_mode ? true : false);
		audio_button->signal_pressed().connect(
				sigc::bind(
					sigc::mem_fun(*this,
						&dv_selector_widget::on_audio_selected),
					i));
		audio_button->show();
		attach(*audio_button,
		       column + column_labels, column + column_labels + 1,
		       row + row_audio_button, row + row_audio_button + 1,
		       Gtk::FILL, Gtk::FILL,
		       0, 0);

		if (i < 9)
		{
		    // Make the mnemonic on the label work.  Also make
		    // the numeric keypad and Alt-keys work.
		    pri_video_button->add_accelerator("activate",
						  accel_group_,
						  GDK_KP_1 + i,
						  Gdk::ModifierType(0),
						  Gtk::AccelFlags(0));
		    pri_video_button->add_accelerator("activate",
						  accel_group_,
						  '1' + i,
						  Gdk::ModifierType(0),
						  Gtk::AccelFlags(0));
		    pri_video_button->signal_activate().connect(
			sigc::bind(
			    sigc::mem_fun(
				*this,
				&dv_selector_widget::on_pri_video_selected),
			    i));
			sec_video_button->add_accelerator("activate",
						      accel_group_,
						      '1' + i,
						      Gdk::CONTROL_MASK,
						      Gtk::AccelFlags(0));
		    sec_video_button->add_accelerator("activate",
						      accel_group_,
						      GDK_KP_1 + i,
						      Gdk::CONTROL_MASK,
						      Gtk::AccelFlags(0));
		    sec_video_button->signal_activate().connect(
			sigc::bind(
			    sigc::mem_fun(
				*this,
				&dv_selector_widget::on_sec_video_selected),
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
				   const dv_frame_ptr & source_frame)
{
    if (source_id < thumbnails_.size())
	thumbnails_[source_id]->put_frame(source_frame);
}

void dv_selector_widget::toggle_audio_buttons()
{
    if(expert_mode)
        return;

    std::vector<Gtk::RadioButton*>::iterator it = audio_button_list_.begin();
    for (; it != audio_button_list_.end(); it++) {
	(*it)->set_sensitive(!(*it)->is_sensitive());
    }
}

sigc::signal1<void, mixer::source_id> &
dv_selector_widget::signal_pri_video_selected()
{
    return pri_video_selected_signal_;
}

sigc::signal1<void, mixer::source_id> &
dv_selector_widget::signal_sec_video_selected()
{
    return sec_video_selected_signal_;
}

sigc::signal1<void, mixer::source_id> &
dv_selector_widget::signal_audio_selected()
{
    return audio_selected_signal_;
}

void dv_selector_widget::on_pri_video_selected(mixer::source_id source_id)
{
    pri_video_selected_signal_(source_id);
}

void dv_selector_widget::on_sec_video_selected(mixer::source_id source_id)
{
    sec_video_selected_signal_(source_id);
}

void dv_selector_widget::on_audio_selected(mixer::source_id source_id)
{
    audio_selected_signal_(source_id);
}

