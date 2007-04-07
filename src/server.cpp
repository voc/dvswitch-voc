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

class server::connection : public SigC::Object, private mixer::sink
{
public:
    connection(server & server, int socket);
    ~connection();

private:
    struct receive_state
    {
	receive_state()
	    : buffer(0),
	      size(0),
	      handler(0)
	{}
	receive_state(uint8_t * buffer, std::size_t size,
		      receive_state (connection::*handler)())
	    : buffer(buffer),
	      size(size),
	      handler(handler)
	{}
	uint8_t * buffer;
	std::size_t size;
	receive_state (connection::*handler)();
    };

    bool do_receive(Glib::IOCondition);
    receive_state identify_client_type();
    receive_state handle_source_sequence();
    receive_state handle_source_frame();
    receive_state handle_unexpected_input();

    virtual void put_frame(const mixer::frame_ptr & frame);
    virtual void cut();

    server & server_;
    int socket_;
    receive_state receive_state_;
    enum { type_unknown, type_source, type_sink } type_;
    union
    {
	uint8_t type_buffer_[4];
	struct
	{
	    mixer::source_id source_id_;
	    dv_decoder_t * decoder_;
	};
	mixer::sink_id sink_id_;
    };
    mixer::frame_ptr frame_;
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
      receive_state_(type_buffer_,
		     sizeof(type_buffer_),
		     &connection::identify_client_type),
      type_(type_unknown)
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
    if (type_ == type_source)
    {
	dv_decoder_free(decoder_);
	server_.mixer_.remove_source(source_id_);
    }
    else if (type_ == type_sink)
    {
	server_.mixer_.remove_sink(sink_id_);
    }

    close(socket_);
}

bool server::connection::do_receive(Glib::IOCondition condition)
{
    bool successful = false;

    if (condition & Glib::IO_IN)
    {
	bool should_retry;

	do
	{
	    should_retry = false;

	    ssize_t received_size = read(socket_,
					 receive_state_.buffer,
					 receive_state_.size);
	    if (received_size > 0)
	    {
		receive_state_.buffer += received_size;
		receive_state_.size -= received_size;
		if (receive_state_.size == 0)
		{
		    receive_state_ = (this->*(receive_state_.handler))();
		    successful = receive_state_.handler != 0;
		    should_retry = successful; // there may be more available
		}
		else
		{
		    successful = true;
		}
	    }
	    else if (received_size == -1 && errno == EWOULDBLOCK)
	    {
		// This is expected when the socket buffer is empty
		successful = true;
	    }
	}
	while (should_retry);
    }

    if (!successful)
    {
	// XXX We should distinguish several kinds of failure: network
	// problems, normal disconnection, protocol violation, and
	// resource allocation failure.
	std::cerr << "WARN: Lost connection from source " << 1 + source_id_
		  << "\n";
	server_.disconnect(this);
    }

    return successful;
}

server::connection::receive_state server::connection::identify_client_type()
{
    // New sources should send 'SORC' as a greeting.
    // Old sources will just start sending DIF directly.
    if (std::memcmp(type_buffer_, "SORC", 4) == 0
	|| ((type_buffer_[0] >> 5) == 0    // header block
	    && (type_buffer_[1] >> 4) == 0 // sequence 0
	    && type_buffer_[2] == 0))      // block 0
    {
	std::size_t received_size =
	    (type_buffer_[0] == 'S') ? 0 : sizeof(type_buffer_);

	if ((frame_ = server_.mixer_.allocate_frame())
	    && (decoder_ = dv_decoder_new(0, true, true)))
	{
	    type_ = type_source;
	    source_id_ = server_.mixer_.add_source();

	    if (received_size)
	    {
		std::memcpy(frame_->buffer,
			    type_buffer_, sizeof(type_buffer_));
		received_size = sizeof(type_buffer_);
	    }

	    return receive_state(frame_->buffer + received_size,
				 DIF_SEQUENCE_SIZE - received_size,
				 &connection::handle_source_sequence);
	}
    }
    // Sinks should send 'SINK' as a greeting (and then nothing else).
    else if (std::memcmp(type_buffer_, "SINK", 4) == 0)
    {
	type_ = type_sink;
	sink_id_ = server_.mixer_.add_sink(this);
	static uint8_t dummy;
	return receive_state(&dummy,
			     1,
			     &connection::handle_unexpected_input);
    }

    return receive_state();
}

server::connection::receive_state server::connection::handle_source_sequence()
{
    if (dv_parse_header(decoder_, frame_->buffer) >= 0)
    {
	frame_->system = decoder_->system;
	frame_->size = decoder_->frame_size;
	return receive_state(frame_->buffer + DIF_SEQUENCE_SIZE,
			     frame_->size - DIF_SEQUENCE_SIZE,
			     &connection::handle_source_frame);
    }

    return receive_state();
}

server::connection::receive_state server::connection::handle_source_frame()
{
    server_.mixer_.put_frame(source_id_, frame_);
    frame_.reset();
    frame_ = server_.mixer_.allocate_frame();
    return receive_state(frame_->buffer,
			 DIF_SEQUENCE_SIZE,
			 &connection::handle_source_sequence);
}

server::connection::receive_state server::connection::handle_unexpected_input()
{
    return receive_state();
}

void server::connection::put_frame(const mixer::frame_ptr &)
{
    // TODO
}

void server::connection::cut()
{
    // TODO
}
