// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

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
    const int display_width = 768;
    const int display_height = 576;
}

dv_display_widget::dv_display_widget()
    : decoder_(dv_decoder_new(0, true, true)),
      xv_port_(XvPortID(-1)),
      xv_image_(0),
      xv_shm_info_(0)
{
    set_app_paintable(true);
    set_double_buffered(false);
    set_size_request(display_width, display_height);

    dv_set_quality(decoder_, DV_QUALITY_BEST);
}

dv_display_widget::~dv_display_widget()
{
    dv_decoder_free(decoder_);
}

void dv_display_widget::set_xv_port(uint32_t port)
{
    Display * display = get_x_display(*this);
    XvImage * xv_image = static_cast<XvImage *>(xv_image_);
    XShmSegmentInfo * xv_shm_info =
	static_cast<XShmSegmentInfo *>(xv_shm_info_);

    if (port == XvPortID(-1))
    {
	assert(xv_port_ != XvPortID(-1));
	XvStopVideo(display, xv_port_, get_x_window(*this));
	if (xv_shm_info->shmaddr)
	    shmdt(xv_shm_info->shmaddr);
	delete xv_shm_info;
	xv_shm_info = 0;
	free(xv_image);
	xv_image = 0;
    }
    else
    {
	assert(xv_port_ == XvPortID(-1));

	xv_shm_info = new XShmSegmentInfo;
	// XXX error checking
	xv_image = XvShmCreateImage(display, port, pixel_format_id, 0,
				    frame_max_width, frame_max_height,
				    xv_shm_info);
	xv_shm_info->shmid = shmget(IPC_PRIVATE, xv_image->data_size,
				    IPC_CREAT | 0777);
	xv_shm_info->shmaddr =
	    static_cast<char *>(shmat(xv_shm_info->shmid, 0, 0));
	xv_image->data = xv_shm_info->shmaddr;
	XShmAttach(display, xv_shm_info);
	shmctl(xv_shm_info->shmid, IPC_RMID, 0);
   }

    xv_image_ = xv_image;
    xv_shm_info_ = xv_shm_info;
    xv_port_ = port;
}

bool dv_display_widget::try_update()
{
    XvImage * xv_image = static_cast<XvImage *>(xv_image_);
    mixer::frame_ptr dv_frame;
    {
	boost::mutex::scoped_lock lock(dv_frame_mutex_);
	dv_frame = dv_frame_;
	dv_frame_.reset();
    }

    if (xv_image && dv_frame && dv_frame->serial_num != decoded_serial_num_)
    {
	dv_parse_header(decoder_, dv_frame->buffer);
	assert(xv_image->num_planes == 1);
	uint8_t * pixels[1] = {
	    reinterpret_cast<uint8_t *>(xv_image->data)
	};
	dv_decode_full_frame(decoder_, dv_frame->buffer,
			     e_dv_color_yuv, pixels, xv_image->pitches);
	decoded_serial_num_ = dv_frame->serial_num;

	// XXX should use get_window()->get_internal_paint_info()
	Display * display = get_x_display(*this);
	if (Glib::RefPtr<Gdk::GC> gc = Gdk::GC::create(get_window()))
	{
	    XvShmPutImage(display, xv_port_,
			  get_x_window(*this), gdk_x11_gc_get_xgc(gc->gobj()),
			  xv_image,
			  0, 0, decoder_->width, decoder_->height,
			  0, 0, display_width, display_height,
			  False);
	    XFlush(display);
	}
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
