// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <memory>

#include <sys/ipc.h>
#include <sys/shm.h>

#include "dv_display_widget.hpp"
#include "frame.h"

// X headers come last due to egregious macro pollution.
#include "gtk_x_utils.hpp"
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>

namespace
{
    const int frame_max_width = 720;
    const int frame_max_height = 576;

    // Assume 4:3 frame ratio for now.
    const int display_width_full = 768;
    const int display_height_full = 576;
    const int display_width_thumb = display_width_full / 4;
    const int display_height_thumb = display_height_full / 4;

    char * allocate_x_shm(Display * display, XShmSegmentInfo * info,
			  std::size_t size)
    {
	char * result = 0;
	if ((info->shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0777)) != -1)
	{
	    info->shmaddr = static_cast<char *>(shmat(info->shmid, 0, 0));
	    if (info->shmaddr != reinterpret_cast<char *>(-1)
		&& XShmAttach(display, info))
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

dv_full_display_widget::dv_full_display_widget()
    : dv_display_widget(DV_QUALITY_BEST),
      xv_port_(XvPortID(-1)),
      xv_image_(0),
      xv_shm_info_(0)
{
    set_size_request(display_width_full, display_height_full);
}

dv_thumb_display_widget::dv_thumb_display_widget()
    : dv_display_widget(DV_QUALITY_FASTEST),
      x_image_(0),
      x_shm_info_(0)
{
    set_size_request(display_width_thumb, display_height_thumb);
}

void dv_full_display_widget::set_xv_port(uint32_t port)
{
    Display * display = get_x_display(*this);
    XvImage * xv_image = static_cast<XvImage *>(xv_image_);
    XShmSegmentInfo * xv_shm_info =
	static_cast<XShmSegmentInfo *>(xv_shm_info_);

    if (port == XvPortID(-1))
    {
	assert(xv_port_ != XvPortID(-1));
	XvStopVideo(display, xv_port_, get_x_window(*this));
	if (xv_image)
	{
	    free_x_shm(xv_shm_info);
	    delete xv_shm_info;
	    xv_shm_info = 0;
	    free(xv_image);
	    xv_image = 0;
	}
    }
    else
    {
	assert(xv_port_ == XvPortID(-1));

	if ((xv_shm_info = new (std::nothrow) XShmSegmentInfo))
	{
	    if ((xv_image = XvShmCreateImage(display, port, pixel_format_id, 0,
					     frame_max_width, frame_max_height,
					     xv_shm_info)))
	    {
		xv_image->data = allocate_x_shm(display, xv_shm_info,
						xv_image->data_size);
		if (!xv_image->data)
		{
		    free(xv_image);
		    xv_image = 0;
		}
	    }

	    if (!xv_image)
	    {
		delete xv_shm_info;
		xv_shm_info = 0;
	    }
	}
   }

    xv_image_ = xv_image;
    xv_shm_info_ = xv_shm_info;
    xv_port_ = port;
}

void dv_display_widget::put_frame(const mixer::frame_ptr & dv_frame)
{
    if (dv_frame->serial_num != decoded_serial_num_)
    {
	pixels_pitch buffer = get_frame_buffer();
	if (!buffer.first)
	    return;

	dv_parse_header(decoder_, dv_frame->buffer);
	dv_decode_full_frame(decoder_, dv_frame->buffer,
			     e_dv_color_yuv, &buffer.first, &buffer.second);
	decoded_serial_num_ = dv_frame->serial_num;

	draw_frame(decoder_->width, decoder_->height);
    }
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

void dv_full_display_widget::draw_frame(unsigned width, unsigned height)
{
    XvImage * xv_image = static_cast<XvImage *>(xv_image_);
    // XXX should use get_window()->get_internal_paint_info()
    Display * display = get_x_display(*this);
    if (Glib::RefPtr<Gdk::GC> gc = Gdk::GC::create(get_window()))
    {
	XvShmPutImage(display, xv_port_,
		      get_x_window(*this), gdk_x11_gc_get_xgc(gc->gobj()),
		      xv_image,
		      0, 0, width, height,
		      0, 0, display_width_full, display_height_full,
		      False);
	XFlush(display);
    }
}

void dv_thumb_display_widget::on_realize()
{
    Gtk::DrawingArea::on_realize();

    Display * display = get_x_display(*this);
    int screen = 0; // XXX should use gdk_x11_screen_get_screen_number
    XVisualInfo visual_info;
    if (XMatchVisualInfo(display, screen, 24, DirectColor, &visual_info))
    {
	if (XShmSegmentInfo * x_shm_info = new (std::nothrow) XShmSegmentInfo)
	{
	    // TODO: We actually need to create an RGB buffer at the
	    // display size and a YUY2 buffer at the video frame
	    // size.  But this will do for a rough demo.
	    if (XImage * x_image = XShmCreateImage(
		    display, visual_info.visual, 24, ZPixmap,
		    0, x_shm_info, frame_max_width, frame_max_height))
	    {
		if ((x_image->data = allocate_x_shm(
			 display, x_shm_info,
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

void dv_thumb_display_widget::on_unrealize()
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

    Gtk::DrawingArea::on_unrealize();
}

dv_display_widget::pixels_pitch dv_thumb_display_widget::get_frame_buffer()
{
    if (XImage * x_image = static_cast<XImage *>(x_image_))
    {
	// XXX This needs to return the YUY2 buffer.
	return pixels_pitch(reinterpret_cast<uint8_t *>(x_image->data),
			    x_image->bytes_per_line);
    }
    else
    {
	return pixels_pitch(0, 0);
    }
}

void dv_thumb_display_widget::draw_frame(unsigned width, unsigned height)
{
    // XXX This needs to convert and scale between the two buffers.
    XImage * x_image = static_cast<XImage *>(x_image_);
    // XXX should use get_window()->get_internal_paint_info()
    Display * display = get_x_display(*this);
    if (Glib::RefPtr<Gdk::GC> gc = Gdk::GC::create(get_window()))
    {
	XShmPutImage(display,
		     get_x_window(*this), gdk_x11_gc_get_xgc(gc->gobj()),
		     x_image,
		     0, 0,
		     0, 0, display_width_thumb, display_height_thumb,
		     False);
	XFlush(display);
    }
}
