// Copyright 2009 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <gtkmm/drawingarea.h>

class vu_meter : public Gtk::DrawingArea
{
public:
    vu_meter(int minimum, int maximum);

    void set_level(int level);

private:
    virtual bool on_expose_event(GdkEventExpose *) throw();

    int minimum_, maximum_, level_;
};
