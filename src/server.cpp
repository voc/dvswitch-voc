// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <functional>
#include <iostream>
#include <ostream>

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <libdv/dv.h>

#include "frame.h"
#include "mixer.hpp"
#include "server.hpp"
#include "socket.h"

class server::connection : public SigC::Object
{
public:
    connection(server & server, int socket);
    ~connection();

private:
    bool do_receive(Glib::IOCondition);

    server & server_;
    int socket_;
    mixer::source_id source_id_;
    dv_decoder_t * decoder_;
    mixer::frame_ptr partial_frame_;
};

server::server(const std::string & host, const std::string & port,
	       mixer & mixer)
    : mixer_(mixer),
      listen_socket_(create_listening_socket(host.c_str(), port.c_str()))
{
    Glib::RefPtr<Glib::IOSource> io_source(
	Glib::IOSource::create(listen_socket_, Glib::IO_IN));
    io_source->connect(SigC::slot(*this, &server::do_accept));
    io_source->attach();
}

server::~server()
{
    close(listen_socket_);
    std::list<connection *>::iterator
	it = connections_.begin(), end = connections_.end();
    while (it != end)
	delete *it++;
}

bool server::do_accept(Glib::IOCondition)
{
    int conn_socket = accept(listen_socket_, 0, 0);
    if (conn_socket >= 0)
	connections_.push_back(new connection(*this, conn_socket));
    return true; // call me again
}

void server::disconnect(connection * conn)
{
    connections_.remove(conn);
    delete conn;
}

server::connection::connection(server & server, int socket)
    : server_(server),
      socket_(socket),
      source_id_(server_.mixer_.add_source()),
      decoder_(dv_decoder_new(0, true, true))
{
    fcntl(socket_, F_SETFL, O_NONBLOCK);
    Glib::RefPtr<Glib::IOSource> io_source(
	Glib::IOSource::create(socket_,
			       Glib::IO_IN | Glib::IO_ERR | Glib::IO_HUP));
    io_source->connect(SigC::slot(*this, &server::connection::do_receive));
    io_source->attach();
}

server::connection::~connection()
{
    dv_decoder_free(decoder_);
    close(socket_);
}

bool server::connection::do_receive(Glib::IOCondition condition)
{
    bool successful = true;

    if (condition & (Glib::IO_ERR | Glib::IO_HUP))
    {
	successful = false;
    }
    else
    {
	for (;;)
	{
	    if (!partial_frame_)
	    {
		partial_frame_ = server_.mixer_.allocate_frame();
		partial_frame_->system = e_dv_system_none;
		partial_frame_->size = 0;
	    }

	    // First we must read the first pack, containing the
	    // header which will indicate the video system and full
	    // frame size.  Then we must read the full frame.
	    // We can determine which state we're in by checking
	    // whether the system is set in our partial frame yet.
	    std::size_t target_size = 
		(partial_frame_->system == e_dv_system_none)
		? DIF_PACK_SIZE
		: decoder_->frame_size;

	    ssize_t received_size = read(
		socket_,
		partial_frame_->buffer + partial_frame_->size,
		target_size - partial_frame_->size);

	    if (received_size > 0)
	    {
		partial_frame_->size += received_size;
		if (partial_frame_->size == target_size)
		{
		    if (partial_frame_->system == e_dv_system_none)
		    {
			// We just finished reading the header.
			if (dv_parse_header(decoder_, partial_frame_->buffer)
			    >= 0)
			    partial_frame_->system = decoder_->system;
			else
			    successful = false;
		    }
		    else
		    {
			// We just finished reading the frame.
			gettimeofday(&partial_frame_->time_received, 0);
			server_.mixer_.put_frame(source_id_, partial_frame_);
			partial_frame_.reset();
		    }
		}
	    }
	    else
	    {
		if (received_size < 0 && errno != EWOULDBLOCK)
		    successful = false;
		break;
	    }
	}
    }

    if (!successful)
	server_.disconnect(this);

    return successful;
}
