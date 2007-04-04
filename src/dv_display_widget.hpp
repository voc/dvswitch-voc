// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_DV_DISPLAY_WIDGET_HPP
#define DVSWITCH_DV_DISPLAY_WIDGET_HPP

#include <gtkmm/drawingarea.h>

#include <libdv/dv.h>

#include "mixer.hpp"

class dv_display_widget : public Gtk::DrawingArea
{
public:
    enum display_type { display_type_full, display_type_thumb };

    explicit dv_display_widget(display_type);
    ~dv_display_widget();

    void set_xv_port(uint32_t);
    void put_frame(const mixer::frame_ptr &);

    static const int pixel_format_id = 0x32595559; // 'YUY2'

private:
    display_type display_type_;

    dv_decoder_t * decoder_;
    unsigned decoded_serial_num_;

    uint32_t xv_port_;
    void * xv_image_;
    void * xv_shm_info_;
};

#endif // !defined(DVSWITCH_DV_DISPLAY_WIDGET_HPP)
