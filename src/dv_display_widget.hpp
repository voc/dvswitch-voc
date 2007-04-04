// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
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

    void put_frame(const mixer::frame_ptr &);

    static const int pixel_format_id = 0x32595559; // 'YUY2'

protected:
    typedef std::pair<uint8_t *, int> pixels_pitch;

private:
    virtual pixels_pitch get_frame_buffer() = 0;
    virtual void draw_frame(unsigned width, unsigned height) = 0;

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
    virtual void draw_frame(unsigned width, unsigned height);

    uint32_t xv_port_;
    void * xv_image_;
    void * xv_shm_info_;
};

class dv_thumb_display_widget : public dv_display_widget
{
public:
    dv_thumb_display_widget();

private:
    virtual pixels_pitch get_frame_buffer();
    virtual void draw_frame(unsigned width, unsigned height);

    virtual void on_realize();
    virtual void on_unrealize();

    void * x_image_;
    void * x_shm_info_;
};

#endif // !defined(DVSWITCH_DV_DISPLAY_WIDGET_HPP)
