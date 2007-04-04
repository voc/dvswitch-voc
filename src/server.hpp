// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_SERVER_HPP
#define DVSWITCH_SERVER_HPP

#include <list>
#include <string>
#include <tr1/memory>

#include <glibmm/main.h>
#include <sigc++/object.h>

#include "mixer.hpp"

class server : public SigC::Object
{
public:
    server(const std::string & host, const std::string & port, mixer & mixer);
    ~server();

private:
    class connection;

    bool do_accept(Glib::IOCondition);
    void disconnect(connection *);

    mixer & mixer_;
    int listen_socket_;
    std::list<connection *> connections_;
};

#endif // !defined(DVSWITCH_SERVER_HPP)
