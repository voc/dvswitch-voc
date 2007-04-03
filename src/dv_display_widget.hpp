// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_DV_DISPLAY_WIDGET_HPP
#define DVSWITCH_DV_DISPLAY_WIDGET_HPP

#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>

#include <gtkmm/drawingarea.h>

#include <libdv/dv.h>

#include "mixer.hpp"

class dv_display_widget : public Gtk::DrawingArea, public mixer::sink
{
public:
    dv_display_widget();
    ~dv_display_widget();

    void set_xv_port(uint32_t);
    bool try_update();

    static const int pixel_format_id = 0x32595559; // 'YUY2'

private:
    virtual void put_frame(const mixer::frame_ptr &);
    virtual void cut();

    boost::mutex dv_frame_mutex_;
    mixer::frame_ptr dv_frame_;

    dv_decoder_t * decoder_;
    unsigned decoded_serial_num_;

    uint32_t xv_port_;
    void * xv_image_;
    void * xv_shm_info_;
};

#endif // !defined(DVSWITCH_DV_DISPLAY_WIDGET_HPP)
