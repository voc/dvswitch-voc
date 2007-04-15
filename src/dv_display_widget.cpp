// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <iostream>
#include <memory>
#include <ostream>

#include <sys/ipc.h>
#include <sys/shm.h>

#include "dv_display_widget.hpp"
#include "frame.h"
#include "video_effect.h"

// X headers come last due to egregious macro pollution.
#include <gdk/gdkx.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>

namespace
{
    // Assume 4:3 frame ratio for now.
    const unsigned display_width_full = FRAME_HEIGHT_MAX * 4 / 3;
    const unsigned display_height_full = FRAME_HEIGHT_MAX;
    const unsigned display_width_thumb = display_width_full / 4;
    const unsigned display_height_thumb = display_height_full / 4;

    const uint32_t invalid_xv_port = uint32_t(-1);

    Display * get_x_display(Gtk::Widget & widget)
    {
	Glib::RefPtr<Gdk::Window> window(widget.get_window());
	assert(window);
	return gdk_x11_drawable_get_xdisplay(window->gobj());
    }

    Window get_x_window(Gtk::Widget & widget)
    {
	Glib::RefPtr<Gdk::Window> window(widget.get_window());
	assert(window);
	return gdk_x11_drawable_get_xid(window->gobj());
    }

    char * allocate_x_shm(Display * x_display, XShmSegmentInfo * info,
			  std::size_t size)
    {
	char * result = 0;
	if ((info->shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0777)) != -1)
	{
	    info->shmaddr = static_cast<char *>(shmat(info->shmid, 0, 0));
	    if (info->shmaddr != reinterpret_cast<char *>(-1)
		&& XShmAttach(x_display, info))
		result = info->shmaddr;
	    shmctl(info->shmid, IPC_RMID, 0);
	}
	return result;
    }

    void free_x_shm(XShmSegmentInfo * info)
    {
	shmdt(info->shmaddr);
    }
}

// dv_display_widget

struct dv_display_widget::drawing_context
{
    Display * x_display;
    Window x_window;
    GC x_gc;
};

dv_display_widget::dv_display_widget(int quality)
    : decoder_(dv_decoder_new(0, true, true)),
      decoded_serial_num_(-1)
{
    set_app_paintable(true);
    set_double_buffered(false);
    dv_set_quality(decoder_, quality);
}

dv_display_widget::~dv_display_widget()
{
    dv_decoder_free(decoder_);
}

void dv_display_widget::put_frame(const mixer::frame_ptr & dv_frame)
{
    if (!is_realized())
	return;

    if (dv_frame->serial_num != decoded_serial_num_)
    {
	pixels_pitch buffer = get_frame_buffer();
	if (!buffer.first)
	    return;

	dv_parse_header(decoder_, dv_frame->buffer);
	dv_decode_full_frame(decoder_, dv_frame->buffer,
			     e_dv_color_yuv, &buffer.first, &buffer.second);
	decoded_serial_num_ = dv_frame->serial_num;

	if (Glib::RefPtr<Gdk::GC> gc = Gdk::GC::create(get_window()))
	{
	    drawing_context context = {
		get_x_display(*this),
		get_x_window(*this),
		gdk_x11_gc_get_xgc(gc->gobj())
	    };
	    assert(decoder_->width == FRAME_WIDTH);
	    draw_frame(context, decoder_->height);
	}
    }
}

// dv_full_display_widget

dv_full_display_widget::dv_full_display_widget()
    : dv_display_widget(DV_QUALITY_BEST),
      xv_port_(invalid_xv_port),
      xv_image_(0),
      xv_shm_info_(0)
{
    set_size_request(display_width_full, display_height_full);
}

void dv_full_display_widget::on_realize() throw()
{
    dv_display_widget::on_realize();

    assert(xv_port_ == invalid_xv_port && xv_image_ == 0);

    Display * x_display = get_x_display(*this);

    unsigned adaptor_count;
    XvAdaptorInfo * adaptor_info;

    if (XvQueryAdaptors(x_display, get_x_window(*this),
			&adaptor_count, &adaptor_info) != Success)
    {
	std::cerr << "ERROR: XvQueryAdaptors() failed\n";
	return;
    }

    // Search for a suitable adaptor.
    unsigned i;
    for (i = 0; i != adaptor_count; ++i)
    {
	if (!(adaptor_info[i].type & XvImageMask))
	    continue;
	int format_count;
	XvImageFormatValues * format_info =
	    XvListImageFormats(x_display, adaptor_info[i].base_id,
			       &format_count);
	if (!format_info)
	    continue;
	for (int j = 0; j != format_count; ++j)
	    if (format_info[j].id == FRAME_PIXEL_FORMAT)
		goto end_adaptor_loop;
    }
end_adaptor_loop:
    if (i == adaptor_count)
    {
	std::cerr << "ERROR: No Xv adaptor for this display supports "
		  << char(FRAME_PIXEL_FORMAT >> 24)
		  << char((FRAME_PIXEL_FORMAT >> 16) & 0xFF)
		  << char((FRAME_PIXEL_FORMAT >> 8) & 0xFF)
		  << char(FRAME_PIXEL_FORMAT & 0xFF)
		  << " format\n";
    }
    else
    {
	// Try to allocate a port.
	unsigned j;
	for (j = 0; j != adaptor_info[i].num_ports; ++j)
	{
	    XvPortID port = adaptor_info[i].base_id + i;
	    if (XvGrabPort(x_display, port, CurrentTime) == Success)
	    {
		xv_port_ = port;
		break;
	    }
	}
	if (j == adaptor_info[i].num_ports)
	    std::cerr << "ERROR: Could not grab an Xv port\n";
    }

    XvFreeAdaptorInfo(adaptor_info);

    if (xv_port_ == invalid_xv_port)
	return;

    if (XShmSegmentInfo * xv_shm_info = new (std::nothrow) XShmSegmentInfo)
    {
	if (XvImage * xv_image =
	    XvShmCreateImage(x_display, xv_port_, FRAME_PIXEL_FORMAT, 0,
			     FRAME_WIDTH, FRAME_HEIGHT_MAX,
			     xv_shm_info))
	{
	    if ((xv_image->data = allocate_x_shm(x_display, xv_shm_info,
						 xv_image->data_size)))
	    {
		xv_image_ = xv_image;
		xv_shm_info_ = xv_shm_info;
	    }
	    else
	    {
		free(xv_image);
		delete xv_shm_info;
	    }
	}
	else
	{
	    delete xv_shm_info;
	}
    }
}

void dv_full_display_widget::on_unrealize() throw()
{
    if (xv_port_ != invalid_xv_port)
    {
	Display * x_display = get_x_display(*this);

	XvStopVideo(x_display, xv_port_, get_x_window(*this));

	if (XvImage * xv_image = static_cast<XvImage *>(xv_image_))
	{
	    xv_image_ = 0;
	    free(xv_image);
	    XShmSegmentInfo * xv_shm_info =
		static_cast<XShmSegmentInfo *>(xv_shm_info_);
	    xv_shm_info_ = 0;
	    free_x_shm(xv_shm_info);
	    delete xv_shm_info;
	}

	XvUngrabPort(x_display, xv_port_, CurrentTime);
	xv_port_ = invalid_xv_port;
    }

    dv_display_widget::on_unrealize();
}

dv_display_widget::pixels_pitch dv_full_display_widget::get_frame_buffer()
{
    if (XvImage * xv_image = static_cast<XvImage *>(xv_image_))
    {
	assert(xv_image->num_planes == 1);
	return pixels_pitch(reinterpret_cast<uint8_t *>(xv_image->data),
			    xv_image->pitches[0]);
    }
    else
    {
	return pixels_pitch(0, 0);
    }
}

void dv_full_display_widget::draw_frame(const drawing_context & context,
					unsigned height)
{
    XvImage * xv_image = static_cast<XvImage *>(xv_image_);
    frame_decoded_ref frame_ref = {
	reinterpret_cast<uint8_t *>(xv_image->data),
	xv_image->pitches[0],
	height
    };
    video_effect_show_title_safe(frame_ref);
	    
    // XXX should use get_window()->get_internal_paint_info()
    XvShmPutImage(context.x_display, xv_port_, context.x_window, context.x_gc,
		  static_cast<XvImage *>(xv_image_),
		  0, 0, FRAME_WIDTH, height,
		  0, 0, display_width_full, display_height_full,
		  False);
    XFlush(context.x_display);
}

// dv_thumb_display_widget

dv_thumb_display_widget::dv_thumb_display_widget()
    : dv_display_widget(DV_QUALITY_FASTEST),
      frame_buffer_(new uint8_t[FRAME_BYTES_PER_PIXEL * FRAME_WIDTH
				* FRAME_HEIGHT_MAX]),
      x_image_(0),
      x_shm_info_(0)
{
    set_size_request(display_width_thumb, display_height_thumb);
}

dv_thumb_display_widget::~dv_thumb_display_widget()
{
    delete[] frame_buffer_;
}

void dv_thumb_display_widget::on_realize() throw()
{
    dv_display_widget::on_realize();

    Display * x_display = get_x_display(*this);
    int screen = 0; // XXX should use gdk_x11_screen_get_screen_number
    XVisualInfo visual_info;
    if (XMatchVisualInfo(x_display, screen, 24, DirectColor, &visual_info))
    {
	if (XShmSegmentInfo * x_shm_info = new (std::nothrow) XShmSegmentInfo)
	{
	    if (XImage * x_image = XShmCreateImage(
		    x_display, visual_info.visual, 24, ZPixmap,
		    0, x_shm_info, display_width_thumb, display_height_thumb))
	    {
		if ((x_image->data = allocate_x_shm(
			 x_display, x_shm_info,
			 x_image->height * x_image->bytes_per_line)))
		{
		    x_image_ = x_image;
		    x_shm_info_ = x_shm_info;
		}
		else
		{
		    free(x_image);
		}
	    }
	    if (!x_shm_info_)
		delete x_shm_info;
	}
    }
}

void dv_thumb_display_widget::on_unrealize() throw()
{
    if (XImage * x_image = static_cast<XImage *>(x_image_))
    {
	XShmSegmentInfo * x_shm_info =
	    static_cast<XShmSegmentInfo *>(x_shm_info_);
	free_x_shm(x_shm_info);
	delete x_shm_info;
	x_shm_info_ = 0;
	free(x_image);
	x_image = 0;
    }

    dv_display_widget::on_unrealize();
}

dv_display_widget::pixels_pitch dv_thumb_display_widget::get_frame_buffer()
{
    return pixels_pitch(frame_buffer_, FRAME_BYTES_PER_PIXEL * FRAME_WIDTH);
}

void dv_thumb_display_widget::draw_frame(const drawing_context & context,
					 unsigned height)
{
    XImage * x_image = static_cast<XImage *>(x_image_);

    // Scale the image down.  Actually this is scaling up because the
    // decoded frame is really blocks of 8x8 pixels anyway, so we can
    // use Bresenham's algorithm.

    assert(x_image->bits_per_pixel == 24 || x_image->bits_per_pixel == 32);

    static const unsigned block_size = 8;
    static const unsigned width_blocks = FRAME_WIDTH / block_size;
    const unsigned height_blocks = height / block_size;
    assert(width_blocks <= display_width_thumb);
    assert(height_blocks <= display_height_thumb);
    unsigned y_in = 0, y_out = 0, y_error = height_blocks / 2;
    do
    {
	const uint8_t * in =
	    frame_buffer_ + FRAME_BYTES_PER_PIXEL * FRAME_WIDTH * y_in;
	uint8_t * out = reinterpret_cast<uint8_t *>(
	    x_image->data + x_image->bytes_per_line * y_out);
	uint8_t * out_row_end =
	    out + x_image->bits_per_pixel / 8 * display_width_thumb;
	unsigned x_error = width_blocks / 2;
	uint8_t in_value = *in; // read first Y component
	do
	{
	    // Write Y component to each byte of the pixel
	    *out++ = in_value;
	    *out++ = in_value;
	    *out++ = in_value;
	    if (x_image->bits_per_pixel == 32)
		*out++ = in_value;

	    x_error += width_blocks;
	    if (x_error >= display_width_thumb)
	    {
		in += FRAME_BYTES_PER_PIXEL * block_size; // next block
		in_value = *in; // read first Y component
		x_error -= display_width_thumb;
	    }
	}
	while (out != out_row_end);
	
	y_error += height_blocks;
	if (y_error >= display_height_thumb)
	{
	    y_in += block_size;
	    y_error -= display_height_thumb;
	}
	++y_out;
    }
    while (y_out != display_height_thumb);

    // XXX should use get_window()->get_internal_paint_info()
    XShmPutImage(context.x_display, context.x_window, context.x_gc,
		 x_image,
		 0, 0,
		 0, 0, display_width_thumb, display_height_thumb,
		 False);
    XFlush(context.x_display);
}
