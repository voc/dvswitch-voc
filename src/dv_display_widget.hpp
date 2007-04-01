// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_DV_DISPLAY_WIDGET_HPP
#define DVSWITCH_DV_DISPLAY_WIDGET_HPP

#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>

#include <gdkmm/pixbuf.h>
#include <gtkmm/image.h>

#include <libdv/dv.h>

#include "mixer.hpp"

class dv_display_widget : public Gtk::Image, public mixer::sink
{
public:
    dv_display_widget();
    ~dv_display_widget();

    bool try_update();

private:
    virtual void put_frame(const mixer::frame_ptr &);
    virtual void cut();

    boost::mutex dv_frame_mutex_;
    mixer::frame_ptr dv_frame_;

    dv_decoder_t * decoder_;
    unsigned decoded_serial_num_;
};

#endif // !defined(DVSWITCH_DV_DISPLAY_WIDGET_HPP)
