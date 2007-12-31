// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_DV_DISPLAY_WIDGET_HPP
#define DVSWITCH_DV_DISPLAY_WIDGET_HPP

#include <utility>

#include <gtkmm/drawingarea.h>

#include <libdv/dv.h>

#include "mixer.hpp"

class dv_display_widget : public Gtk::DrawingArea
{
public:
    explicit dv_display_widget(int quality);
    ~dv_display_widget();

    void put_frame(const mixer::dv_frame_ptr &);

protected:
    typedef std::pair<uint8_t *, int> pixels_pitch;
    struct rectangle
    {
	unsigned left, top;
	unsigned width, height;
	unsigned pixel_width, pixel_height;
    };

private:
    virtual pixels_pitch get_frame_buffer() = 0;
    virtual void put_frame_buffer(const rectangle &) = 0;

    dv_decoder_t * decoder_;
    unsigned decoded_serial_num_;
};

class dv_full_display_widget : public dv_display_widget
{
public:
    dv_full_display_widget();

    void set_xv_port(uint32_t);

private:
    virtual pixels_pitch get_frame_buffer();
    virtual void put_frame_buffer(const rectangle &);

    virtual bool on_expose_event(GdkEventExpose *) throw();
    virtual void on_realize() throw();
    virtual void on_unrealize() throw();

    uint32_t xv_port_;
    void * xv_image_;
    void * xv_shm_info_;
    rectangle source_rect_;
    unsigned dest_width_, dest_height_;
};

class dv_thumb_display_widget : public dv_display_widget
{
public:
    dv_thumb_display_widget();
    ~dv_thumb_display_widget();

private:
    virtual pixels_pitch get_frame_buffer();
    virtual void put_frame_buffer(const rectangle &);

    virtual bool on_expose_event(GdkEventExpose *) throw();
    virtual void on_realize() throw();
    virtual void on_unrealize() throw();

    uint8_t * frame_buffer_;
    void * x_image_;
    void * x_shm_info_;
    unsigned dest_width_, dest_height_;
};

#endif // !defined(DVSWITCH_DV_DISPLAY_WIDGET_HPP)
