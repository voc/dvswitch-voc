// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_DV_DISPLAY_WIDGET_HPP
#define DVSWITCH_DV_DISPLAY_WIDGET_HPP

#include <memory>
#include <utility>

#include <gtkmm/drawingarea.h>

#include "auto_codec.hpp"
#include "frame.h"
#include "frame_pool.hpp"
#include "geometry.hpp"

class dv_display_widget : public Gtk::DrawingArea
{
public:
    explicit dv_display_widget(int lowres = 0);
    ~dv_display_widget();

    void put_frame(const dv_frame_ptr &);
    void put_frame(const raw_frame_ptr &);

protected:
    struct display_region : rectangle
    {
	unsigned pixel_width, pixel_height;
    };

    auto_codec decoder_;

private:
    display_region get_display_region(const dv_system * system,
				      enum dv_frame_aspect frame_aspect);
    virtual AVFrame * get_frame_header() = 0;
    virtual AVFrame * get_frame_buffer(AVFrame * header,
				       PixelFormat pix_fmt, unsigned height) = 0;
    virtual void put_frame_buffer(const display_region &) = 0;

    static int get_buffer(AVCodecContext *, AVFrame *);
    static void release_buffer(AVCodecContext *, AVFrame *);
    static int reget_buffer(AVCodecContext *, AVFrame *);

    unsigned decoded_serial_num_;
};

class dv_full_display_widget : public dv_display_widget
{
public:
    dv_full_display_widget();

private:
    bool try_init_xvideo(PixelFormat pix_fmt, unsigned height) throw();
    void fini_xvideo() throw();

    virtual AVFrame * get_frame_header();
    virtual AVFrame * get_frame_buffer(AVFrame * header,
				       PixelFormat pix_fmt, unsigned height);
    virtual void put_frame_buffer(const display_region &);

    virtual bool on_expose_event(GdkEventExpose *) throw();
    virtual void on_unrealize() throw();

    PixelFormat pix_fmt_;
    unsigned height_;
    uint32_t xv_port_;
    void * xv_image_;
    void * xv_shm_info_;
    AVFrame frame_header_;
    display_region source_region_;
    unsigned dest_width_, dest_height_;
};

class dv_thumb_display_widget : public dv_display_widget
{
public:
    dv_thumb_display_widget();
    ~dv_thumb_display_widget();

private:
    struct raw_frame_thumb;

    bool try_init_xshm(PixelFormat pix_fmt, unsigned height) throw();
    void fini_xshm() throw();

    virtual AVFrame * get_frame_header();
    virtual AVFrame * get_frame_buffer(AVFrame * header,
				       PixelFormat pix_fmt, unsigned height);
    virtual void put_frame_buffer(const display_region &);

    virtual bool on_expose_event(GdkEventExpose *) throw();
    virtual void on_unrealize() throw();

    std::auto_ptr<raw_frame_thumb> raw_frame_;
    void * x_image_;
    void * x_shm_info_;
    unsigned dest_width_, dest_height_;
};

#endif // !defined(DVSWITCH_DV_DISPLAY_WIDGET_HPP)
