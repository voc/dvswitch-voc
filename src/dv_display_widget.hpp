// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_DV_DISPLAY_WIDGET_HPP
#define DVSWITCH_DV_DISPLAY_WIDGET_HPP

#include <utility>

#include <gtkmm/drawingarea.h>

#include "auto_codec.hpp"
#include "frame.h"
#include "frame_pool.hpp"

class dv_display_widget : public Gtk::DrawingArea
{
public:
    dv_display_widget();
    ~dv_display_widget();

    void put_frame(const dv_frame_ptr &);
    void put_frame(const raw_frame_ptr &);

protected:
    struct rectangle
    {
	unsigned left, top;
	unsigned width, height;
	unsigned pixel_width, pixel_height;
    };

    auto_codec decoder_;

private:
    rectangle get_source_rect(const dv_system * system,
			      enum dv_frame_aspect frame_aspect);
    virtual AVFrame * get_frame_buffer() = 0;
    virtual void put_frame_buffer(const rectangle &) = 0;

    unsigned decoded_serial_num_;
};

class dv_full_display_widget : public dv_display_widget
{
public:
    dv_full_display_widget();

private:
    virtual AVFrame * get_frame_buffer();
    virtual void put_frame_buffer(const rectangle &);

    static int get_buffer(AVCodecContext *, AVFrame *);
    static void release_buffer(AVCodecContext *, AVFrame *);
    static int reget_buffer(AVCodecContext *, AVFrame *);

    virtual bool on_expose_event(GdkEventExpose *) throw();
    virtual void on_realize() throw();
    virtual void on_unrealize() throw();

    uint32_t xv_port_;
    void * xv_image_;
    void * xv_shm_info_;
    AVFrame frame_header_;
    rectangle source_rect_;
    unsigned dest_width_, dest_height_;
};

class dv_thumb_display_widget : public dv_display_widget
{
public:
    dv_thumb_display_widget();
    ~dv_thumb_display_widget();

private:
    virtual AVFrame * get_frame_buffer();
    virtual void put_frame_buffer(const rectangle &);

    virtual bool on_expose_event(GdkEventExpose *) throw();
    virtual void on_realize() throw();
    virtual void on_unrealize() throw();

    raw_frame_ptr raw_frame_;
    void * x_image_;
    void * x_shm_info_;
    unsigned dest_width_, dest_height_;
};

#endif // !defined(DVSWITCH_DV_DISPLAY_WIDGET_HPP)
