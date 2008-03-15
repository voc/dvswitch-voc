// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <cassert>
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
    unsigned div_round_nearest(unsigned numer, unsigned denom)
    {
	return (numer + denom / 2) / denom;
    }

    const unsigned thumb_scale_denom = 4;

    const uint32_t invalid_xv_port = uint32_t(-1);

    Display * get_x_display(const Glib::RefPtr<Gdk::Drawable> & drawable)
    {
	return gdk_x11_drawable_get_xdisplay(drawable->gobj());
    }

    Display * get_x_display(Gtk::Widget & widget)
    {
	Glib::RefPtr<Gdk::Drawable> window(widget.get_window());
	assert(window);
	return get_x_display(window);
    }

    Window get_x_window(const Glib::RefPtr<Gdk::Drawable> & drawable)
    {
	return gdk_x11_drawable_get_xid(drawable->gobj());
    }

    Window get_x_window(Gtk::Widget & widget)
    {
	Glib::RefPtr<Gdk::Drawable> window(widget.get_window());
	assert(window);
	return get_x_window(window);
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

    // XXX These will only work for "PAL" frames.
    // For "NTSC" frames the codec will give us PIX_FMT_YUV411P which
    // we will have to convert to YUY2 or YV16.
    const int wanted_pix_fmt_xvideo = 0x30323449; // 'I420'
    const PixelFormat wanted_pix_fmt_avcodec = PIX_FMT_YUV420P;
}

// dv_display_widget

dv_display_widget::dv_display_widget()
    : decoder_(auto_codec_open(&dvvideo_decoder)),
      decoded_serial_num_(-1)
{
    set_app_paintable(true);
    set_double_buffered(false);
}

dv_display_widget::~dv_display_widget()
{
}

void dv_display_widget::put_frame(const dv_frame_ptr & dv_frame)
{
    if (!is_realized())
	return;

    if (dv_frame->serial_num != decoded_serial_num_)
    {
	const struct dv_system * system = dv_frame_system(dv_frame.get());
	AVCodecContext * decoder = decoder_.get();

	AVFrame * header = get_frame_buffer();
	if (!header)
	    return;

	int got_frame;
	int used_size = avcodec_decode_video(decoder,
					     header, &got_frame,
					     dv_frame->buffer, system->size);
	assert(got_frame && size_t(used_size) == system->size);
	header->opaque = const_cast<void *>(static_cast<const void *>(system));
	decoded_serial_num_ = dv_frame->serial_num;

	rectangle source_rect;

	source_rect.left = (system->frame_width - system->frame_width_used) / 2;
	source_rect.top = 0;
	source_rect.width = system->frame_width_used;
	source_rect.height = system->frame_height;

	if (dv_frame_aspect(dv_frame.get()) == dv_frame_aspect_wide)
	{
	    source_rect.pixel_width = system->pixel_aspect_wide.width;
	    source_rect.pixel_height = system->pixel_aspect_wide.height;
	}
	else
	{
	    source_rect.pixel_width = system->pixel_aspect_normal.width;
	    source_rect.pixel_height = system->pixel_aspect_normal.height;
	}

	put_frame_buffer(source_rect);
	queue_draw();
    }
}

// dv_full_display_widget

dv_full_display_widget::dv_full_display_widget()
    : xv_port_(invalid_xv_port),
      xv_image_(0),
      xv_shm_info_(0)
{
    AVCodecContext * decoder = decoder_.get();
    decoder->opaque = this;
    decoder->get_buffer = get_buffer;
    decoder->release_buffer = release_buffer;
    decoder->reget_buffer = reget_buffer;

    // We don't know what the frame format will be, but assume "PAL"
    // 4:3 frames and therefore an active image size of 702x576 and
    // pixel aspect ratio of 59:54.
    set_size_request(767, 576);
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
	    if (format_info[j].id == wanted_pix_fmt_xvideo)
		goto end_adaptor_loop;
    }
end_adaptor_loop:
    if (i == adaptor_count)
    {
	std::cerr << "ERROR: No Xv adaptor for this display supports "
		  << char(wanted_pix_fmt_xvideo >> 24)
		  << char((wanted_pix_fmt_xvideo >> 16) & 0xFF)
		  << char((wanted_pix_fmt_xvideo >> 8) & 0xFF)
		  << char(wanted_pix_fmt_xvideo & 0xFF)
		  << " format\n";
    }
    else
    {
	// Try to allocate a port.
	unsigned j;
	for (j = 0; j != adaptor_info[i].num_ports; ++j)
	{
	    XvPortID port = adaptor_info[i].base_id + j;
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
	    XvShmCreateImage(x_display, xv_port_, wanted_pix_fmt_xvideo, 0,
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

    if (!xv_image_)
	std::cerr << "ERROR: Could not create Xv image\n";
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

AVFrame * dv_full_display_widget::get_frame_buffer()
{
    return xv_image_ ? &frame_header_ : 0;
}

void dv_full_display_widget::put_frame_buffer(const rectangle & source_rect)
{
    XvImage * xv_image = static_cast<XvImage *>(xv_image_);

    raw_frame_ref frame_ref;
    for (int plane = 0; plane != 4; ++plane)
    {
	if (plane < xv_image->num_planes)
	{
	    frame_ref.planes.data[plane] =
		reinterpret_cast<uint8_t *>(xv_image->data
					    + xv_image->offsets[plane]);
	    frame_ref.planes.linesize[plane] = xv_image->pitches[plane];
	}
	else
	{
	    frame_ref.planes.data[plane] = 0;
	    frame_ref.planes.linesize[plane] = 0;
	}
    }
    frame_ref.pix_fmt = wanted_pix_fmt_avcodec;
    frame_ref.height = source_rect.height;

    video_effect_show_title_safe(frame_ref);

    if (source_rect.pixel_width > source_rect.pixel_height)
    {
	dest_height_ = source_rect.height;
	dest_width_ = div_round_nearest(source_rect.width
				       * source_rect.pixel_width,
				       source_rect.pixel_height);
    }
    else
    {
	dest_width_ = source_rect.width;
	dest_height_ = div_round_nearest(source_rect.height
					* source_rect.pixel_height,
					source_rect.pixel_width);
    }
    source_rect_ = source_rect;
    set_size_request(dest_width_, dest_height_);
}

int dv_full_display_widget::get_buffer(AVCodecContext * context,
				       AVFrame * header)
{
    dv_full_display_widget * widget =
	static_cast<dv_full_display_widget *>(context->opaque);
    XvImage * xv_image = static_cast<XvImage *>(widget->xv_image_);

    assert(context->pix_fmt == wanted_pix_fmt_avcodec);

    for (int plane = 0; plane != 4; ++plane)
    {
	if (plane < xv_image->num_planes)
	{
	    header->data[plane] =
		reinterpret_cast<uint8_t *>(xv_image->data
					    + xv_image->offsets[plane]);
	    header->linesize[plane] = xv_image->pitches[plane];
	}
	else
	{
	    header->data[plane] = 0;
	    header->linesize[plane] = 0;
	}
    }

    header->type = FF_BUFFER_TYPE_USER;
    return 0;
}

void dv_full_display_widget::release_buffer(AVCodecContext *, AVFrame * header)
{
    for (int i = 0; i != 4; ++i)
	header->data[i] = 0;
}

int dv_full_display_widget::reget_buffer(AVCodecContext *, AVFrame *)
{
    return 0;
}

bool dv_full_display_widget::on_expose_event(GdkEventExpose *) throw()
{
    if (!xv_image_)
	return true;

    Glib::RefPtr<Gdk::Drawable> drawable;
    int dest_x, dest_y;
    get_window()->get_internal_paint_info(drawable, dest_x, dest_y);
    drawable->reference(); // get_internal_paint_info() doesn't do this!

    if (Glib::RefPtr<Gdk::GC> gc = Gdk::GC::create(drawable))
    {
	Display * x_display = get_x_display(drawable);
	XvShmPutImage(x_display, xv_port_,
		      get_x_window(drawable),
		      gdk_x11_gc_get_xgc(gc->gobj()),
		      static_cast<XvImage *>(xv_image_),
		      source_rect_.left, source_rect_.top,
		      source_rect_.width, source_rect_.height,
		      dest_x, dest_y,
		      dest_width_, dest_height_,
		      False);
	XFlush(x_display);
    }

    return true;
}

// dv_thumb_display_widget

dv_thumb_display_widget::dv_thumb_display_widget()
    : raw_frame_(allocate_raw_frame()),
      x_image_(0),
      x_shm_info_(0),
      dest_width_(0),
      dest_height_(0)
{
    AVCodecContext * decoder = decoder_.get();
    decoder->get_buffer = raw_frame_get_buffer;
    decoder->release_buffer = raw_frame_release_buffer;
    decoder->reget_buffer = raw_frame_reget_buffer;
    decoder->opaque = raw_frame_.get();

    // We don't know what the frame format will be, but assume "PAL"
    // 4:3 frames and therefore an active image size of 702x576 and
    // pixel aspect ratio of 59:54.
    set_size_request(192, 144);
}

dv_thumb_display_widget::~dv_thumb_display_widget()
{
}

void dv_thumb_display_widget::on_realize() throw()
{
    dv_display_widget::on_realize();

    Display * x_display = get_x_display(*this);

    Glib::RefPtr<Gdk::Drawable> drawable;
    int dest_x, dest_y;
    get_window()->get_internal_paint_info(drawable, dest_x, dest_y);
    Visual * visual =
	gdk_x11_visual_get_xvisual(drawable->get_visual()->gobj());
    int depth = drawable->get_depth();

    if ((visual->c_class == TrueColor || visual->c_class == DirectColor)
	&& (depth == 24 || depth == 32))
    {
	if (XShmSegmentInfo * x_shm_info = new (std::nothrow) XShmSegmentInfo)
	{
	    if (XImage * x_image = XShmCreateImage(
		    x_display, visual, depth, ZPixmap,
		    0, x_shm_info,
		    // Calculate maximum dimensions assuming widest pixel
		    // ratio and full frame (slightly over-conservative).
		    div_round_nearest(FRAME_WIDTH * 118,
				      81 * thumb_scale_denom),
		    div_round_nearest(FRAME_HEIGHT_MAX,
				      thumb_scale_denom)))
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

	if (!x_image_)
	    std::cerr << "ERROR: Could not create Xshm image\n";
    }
    else
    {
	std::cerr << "ERROR: Window does not support 24- or 32-bit colour\n";
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
	x_image_ = 0;
    }

    dv_display_widget::on_unrealize();
}

AVFrame * dv_thumb_display_widget::get_frame_buffer()
{
    return x_image_ ? &raw_frame_->header : 0;
}

void dv_thumb_display_widget::put_frame_buffer(const rectangle & source_rect)
{
    XImage * x_image = static_cast<XImage *>(x_image_);

    dest_width_ = div_round_nearest(source_rect.width
				    * source_rect.pixel_width,
				    source_rect.pixel_height
				    * thumb_scale_denom);
    dest_height_ = div_round_nearest(source_rect.height,
				     thumb_scale_denom);

    // Scale the image down.  Actually this is scaling up because the
    // decoded frame is really blocks of 8x8 pixels anyway, so we can
    // use Bresenham's algorithm.

    assert(x_image->bits_per_pixel == 24 || x_image->bits_per_pixel == 32);

    static const unsigned block_size = 8;
    static const unsigned source_width_blocks = source_rect.width / block_size;
    const unsigned source_height_blocks = source_rect.height / block_size;
    assert(source_width_blocks <= dest_width_);
    assert(source_height_blocks <= dest_height_);
    unsigned source_y = source_rect.top, dest_y = 0;
    unsigned error_y = source_height_blocks / 2;
    do
    {
	const uint8_t * source =
	    raw_frame_->buffer._420.y + FRAME_LINESIZE_4 * source_y
	    + source_rect.left + block_size / 2;
	uint8_t * dest = reinterpret_cast<uint8_t *>(
	    x_image->data + x_image->bytes_per_line * dest_y);
	uint8_t * dest_row_end =
	    dest + x_image->bits_per_pixel / 8 * dest_width_;
	unsigned error_x = source_width_blocks / 2;
	uint8_t source_value = *source; // read first Y component
	do
	{
	    // Write Y component to each byte of the pixel
	    *dest++ = source_value;
	    *dest++ = source_value;
	    *dest++ = source_value;
	    if (x_image->bits_per_pixel == 32)
		*dest++ = source_value;

	    error_x += source_width_blocks;
	    if (error_x >= dest_width_)
	    {
		source += block_size; // next block
		source_value = *source; // read first Y component
		error_x -= dest_width_;
	    }
	}
	while (dest != dest_row_end);
	
	error_y += source_height_blocks;
	if (error_y >= dest_height_)
	{
	    source_y += block_size;
	    error_y -= dest_height_;
	}
	++dest_y;
    }
    while (dest_y != dest_height_);

    set_size_request(dest_width_, dest_height_);
}

bool dv_thumb_display_widget::on_expose_event(GdkEventExpose *) throw()
{
    if (!x_image_ || !dest_width_ || !dest_height_)
	return true;

    Glib::RefPtr<Gdk::Drawable> drawable;
    int dest_x, dest_y;
    get_window()->get_internal_paint_info(drawable, dest_x, dest_y);
    drawable->reference(); // get_internal_paint_info() doesn't do this!

    if (Glib::RefPtr<Gdk::GC> gc = Gdk::GC::create(drawable))
    {
	Display * x_display = get_x_display(drawable);
	XShmPutImage(x_display,
		     get_x_window(drawable),
		     gdk_x11_gc_get_xgc(gc->gobj()),
		     static_cast<XImage *>(x_image_),
		     0, 0,
		     dest_x, dest_y,
		     dest_width_, dest_height_,
		     False);
	XFlush(x_display);
    }

    return true;
}
