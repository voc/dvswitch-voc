// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_SERVER_HPP
#define DVSWITCH_SERVER_HPP

#include <memory>
#include <string>

#include <boost/thread.hpp>

#include "mixer.hpp"

class server
{
public:
    server(const std::string & host, const std::string & port, mixer & mixer);
    ~server();

private:
    class connection;

    void serve();

    mixer & mixer_;
    int listen_socket_;
    int pipe_ends_[2];
    std::auto_ptr<boost::thread> server_thread_;
};

#endif // !defined(DVSWITCH_SERVER_HPP)
