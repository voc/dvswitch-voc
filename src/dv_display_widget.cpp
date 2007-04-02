// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include "dv_display_widget.hpp"
#include "frame.h"

dv_display_widget::dv_display_widget()
    : Gtk::Image(Gdk::Pixbuf::create(
			 Gdk::COLORSPACE_RGB,
			 false, 8, // has_alpha, bits_per_sample
			 720, 576)), // XXX These should be named constants
      decoder_(dv_decoder_new(0, true, true))
{
    dv_set_quality(decoder_, DV_QUALITY_BEST);
}

dv_display_widget::~dv_display_widget()
{
    dv_decoder_free(decoder_);
}

bool dv_display_widget::try_update()
{
    mixer::frame_ptr dv_frame;

    {
	boost::mutex::scoped_lock lock(dv_frame_mutex_);
	dv_frame = dv_frame_;
    }

    if (dv_frame && dv_frame->serial_num != decoded_serial_num_)
    {
	// Decode and force redraw.
	// If we were to decode to YUV we would use three separate
	// pixmaps (the latter two at reduced resolution).  So long
	// as we decode to RGB there's only one.
	Glib::RefPtr<Gdk::Pixbuf> decoded_frame(get_pixbuf());
	uint8_t * pixels[1] = { decoded_frame->get_pixels() };
	int pitches[1] = { decoded_frame->get_rowstride() };
	dv_parse_header(decoder_, dv_frame->buffer);
	dv_decode_full_frame(decoder_, dv_frame->buffer,
			     e_dv_color_rgb, pixels, pitches);
	decoded_serial_num_ = dv_frame->serial_num;
	queue_draw();
    }

    return true; // call me again
}

void dv_display_widget::put_frame(const mixer::frame_ptr & frame)
{
    boost::mutex::scoped_lock lock(dv_frame_mutex_);
    dv_frame_ = frame;
}

void dv_display_widget::cut()
{
    // Ignore
}
