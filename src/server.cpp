// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <cstring>
#include <functional>
#include <iostream>
#include <ostream>
#include <stdexcept>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <boost/bind.hpp>
#include <boost/variant.hpp>

#include <libdv/dv.h>

#include "frame.h"
#include "mixer.hpp"
#include "protocol.h"
#include "server.hpp"
#include "socket.h"

class server::connection : private mixer::sink
{
public:
    enum send_status {
	send_failed,
	sent_some,
	sent_all
    };

    connection(server & server, int socket);
    ~connection();
    bool do_receive();
    send_status do_send();

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

    struct unknown_state
    {
	uint8_t greeting[4];
    };

    struct source_state
    {
	mixer::source_id source_id;
	dv_decoder_t * decoder;
	mixer::frame_ptr frame;
    };

    // This controls access to frames and overflowed in sink state.
    // It's not a member of sink_state because it's non-copiable.
    boost::mutex sink_queue_mutex_;

    struct sink_state
    {
	mixer::sink_id sink_id;
	std::size_t frame_pos;

	ring_buffer<mixer::frame_ptr, 30> frames;
	bool overflowed;
    };

    receive_state identify_client_type();
    receive_state handle_source_sequence();
    receive_state handle_source_frame();
    receive_state handle_unexpected_input();

    virtual void put_frame(const mixer::frame_ptr & frame);

    server & server_;
    int socket_;
    // conn_state_ *must* come before receive_state_ so that we can
    // safely initialise receive_state_ to refer to conn_state_.
    boost::variant<unknown_state, source_state, sink_state> conn_state_;
    receive_state receive_state_;

    struct state_visitor_reporter;
    struct state_visitor_cleaner;
};

class server::connection::state_visitor_reporter
{
public:
    state_visitor_reporter(std::ostream & os)
	: os_(os)
    {}
    void operator()(const unknown_state &) const
    {
	os_ << "unknown client";
    }
    void operator()(const source_state & state) const
    {
	os_ << "source " << 1 + state.source_id;
    }
    void operator()(const sink_state & state) const
    {
	os_ << "sink " << 1 + state.sink_id;
    }
    typedef void result_type;
private:
    std::ostream & os_;
};

class server::connection::state_visitor_cleaner
{
public:
    state_visitor_cleaner(mixer & mixer)
	: mixer_(mixer)
    {}
    void operator()(unknown_state &) const
    {}
    void operator()(source_state & state) const
    {
	dv_decoder_free(state.decoder);
	mixer_.remove_source(state.source_id);
    }
    void operator()(sink_state & state) const
    {
	mixer_.remove_sink(state.sink_id);
    }
    typedef void result_type;
private:
    mixer & mixer_;
};

server::server(const std::string & host, const std::string & port,
	       mixer & mixer)
    : mixer_(mixer),
      listen_socket_(create_listening_socket(host.c_str(), port.c_str()))
{
    if (pipe(pipe_ends_) != 0)
    {
	close(listen_socket_);
	throw std::runtime_error(std::string("pipe: ")
				 .append(std::strerror(errno)));
    }
    fcntl(pipe_ends_[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_ends_[1], F_SETFL, O_NONBLOCK);

    server_thread_.reset(new boost::thread(boost::bind(&server::serve, this)));
}

server::~server()
{
    int quit_message = -1;
    write(pipe_ends_[1], &quit_message, sizeof(int));
    server_thread_->join();

    close(listen_socket_);
    close(pipe_ends_[0]);
    close(pipe_ends_[1]);
}

void server::serve()
{
    std::vector<pollfd> poll_fds(2);
    std::vector<connection *> connections;
    poll_fds[0].fd = pipe_ends_[0];
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = listen_socket_;
    poll_fds[1].events = POLLIN;

    for (;;)
    {
	int count = poll(&poll_fds[0], poll_fds.size(), -1);
	if (count < 0)
	{
	    int error = errno;
	    if (error == EAGAIN || error == EINTR)
		continue;
	    std::cerr << "ERROR: poll: " << std::strerror(errno) << "\n";
	    break;
	}

	// Check message pipe
	if (poll_fds[0].revents & POLLIN)
	{
	    int messages[1024];
	    ssize_t size = read(pipe_ends_[0], messages, sizeof(messages));
	    if (size > 0)
	    {
		for (std::size_t i = 0;
		     (i + 1) * sizeof(int) <= std::size_t(size);
		     ++i)
		{
		    // Each message is either -1 (quit) or the number of an
		    // FD that we now want to write to.
		    if (messages[i] == -1)
			goto exit;

		    for (std::size_t j = 2; j != poll_fds.size(); ++j)
		    {
			if (poll_fds[j].fd == messages[i])
			{
			    poll_fds[j].events |= POLLOUT;
			    break;
			}
		    }
		}
	    }
	}

	// Check listening socket
	if (poll_fds[1].revents & POLLIN)
	{
	    int conn_socket = accept(listen_socket_, 0, 0);
	    if (conn_socket >= 0)
	    {
		fcntl(conn_socket, F_SETFL, O_NONBLOCK);
		connections.push_back(new connection(*this, conn_socket));
		pollfd new_poll_fd = { conn_socket, POLLIN, 0 };
		poll_fds.push_back(new_poll_fd);
	    }
	}

	// Check client connections
	for (std::size_t i = 0; i != connections.size();)
	{
	    short revents = poll_fds[2 + i].revents;
	    bool should_drop = false;
	    try
	    {
		if (revents & (POLLHUP | POLLERR))
		{
		    should_drop = true;
		}
		else if (revents & POLLIN && !connections[i]->do_receive())
		{
		    should_drop = true;
		}
		else if (revents & POLLOUT)
		{
		    switch (connections[i]->do_send())
		    {
		    case connection::send_failed:
			should_drop = true;
			break;
		    case connection::sent_some:
			break;
		    case connection::sent_all:
			poll_fds[2 + i].events &= ~POLLOUT;
		        break;
		    }
		}
	    }
	    catch (std::exception & e)
	    {
		std::cerr << "ERROR: " << e.what() << "\n";
		should_drop = true;
	    }

	    if (should_drop)
	    {
		delete connections[i];
		connections.erase(connections.begin() + i);
		poll_fds.erase(poll_fds.begin() + 2 + i);
	    }
	    else
	    {
		++i;
	    }
	}
    }

exit:
    std::vector<connection *>::iterator
	it = connections.begin(), end = connections.end();
    while (it != end)
	delete *it++;
}

void server::enable_output_polling(int fd)
{
    write(pipe_ends_[1], &fd, sizeof(int));
}

server::connection::connection(server & server, int socket)
    : server_(server),
      socket_(socket),
      receive_state_(boost::get<unknown_state>(conn_state_).greeting,
		     sizeof(unknown_state().greeting),
		     &connection::identify_client_type)
{}

server::connection::~connection()
{
    boost::apply_visitor(state_visitor_cleaner(server_.mixer_), conn_state_);
    close(socket_);
}

bool server::connection::do_receive()
{
    bool successful = false;
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

    if (!successful)
    {
	// XXX We should distinguish several kinds of failure: network
	// problems, normal disconnection, protocol violation, and
	// resource allocation failure.
	std::cerr << "WARN: Dropping connection from ";
	boost::apply_visitor(state_visitor_reporter(std::cerr), conn_state_);
	std::cerr << "\n";
    }

    return successful;
}

server::connection::send_status server::connection::do_send()
{
    sink_state & state = boost::get<sink_state>(conn_state_);

    send_status result = send_failed;
    bool finished_frame = false;

    do
    {
	mixer::frame_ptr frame;
	{
	    boost::mutex::scoped_lock lock(sink_queue_mutex_);
	    if (state.overflowed)
		break;
	    if (finished_frame)
	    {
		state.frames.pop();
		finished_frame = false;
	    }
	    if (state.frames.empty())
	    {
		result = sent_all;
		break;
	    }
	    frame = state.frames.front();
	}

	uint8_t frame_header[SINK_FRAME_HEADER_SIZE] = {};
	frame_header[SINK_FRAME_CUT_FLAG_POS] = frame->cut_before ? 'C' : 0;
	// rest of header left as zero for expansion
	iovec io_vector[2] = {
	    { frame_header,  SINK_FRAME_HEADER_SIZE },
	    { frame->buffer, frame->size }
	};
	int done_count = 0;
	std::size_t rel_pos = state.frame_pos;
	while (rel_pos >= io_vector[done_count].iov_len)
	{
	    rel_pos -= io_vector[done_count].iov_len;
	    ++done_count;
	}
	io_vector[done_count].iov_base =
	    static_cast<char *>(io_vector[done_count].iov_base) + rel_pos;
	io_vector[done_count].iov_len -= rel_pos;
	  
	ssize_t sent_size = writev(socket_,
				   io_vector + done_count,
				   2 - done_count);
	if (sent_size > 0)
	{
	    state.frame_pos += sent_size;
	    if (state.frame_pos == SINK_FRAME_HEADER_SIZE + frame->size)
	    {
		finished_frame = true;
		state.frame_pos = 0;
	    }
	    result = sent_some;
	}
	else if (sent_size == -1 && errno == EWOULDBLOCK)
	{
	    result = sent_some;
	}
    }
    while (finished_frame);

    if (result == send_failed)
    {
	// XXX We should distinguish several kinds of failure: network
	// problems, normal disconnection, protocol violation, and
	// resource allocation failure.
	std::cerr << "WARN: Dropping connection from sink "
		  << 1 + state.sink_id << "\n";
    }

    return result;
}

server::connection::receive_state server::connection::identify_client_type()
{
    unknown_state old_state = boost::get<unknown_state>(conn_state_);

    // New sources should send 'SORC' as a greeting.
    // Old sources will just start sending DIF directly.
    if (std::memcmp(old_state.greeting, GREETING_SOURCE, GREETING_SIZE) == 0
	|| ((old_state.greeting[0] >> 5) == 0    // header block
	    && (old_state.greeting[1] >> 4) == 0 // sequence 0
	    && old_state.greeting[2] == 0))      // block 0
    {
	source_state new_state;
	if ((new_state.frame = server_.mixer_.allocate_frame())
	    && (new_state.decoder = dv_decoder_new(0, true, true)))
	{
	    new_state.source_id = server_.mixer_.add_source();
	    conn_state_ = new_state;

	    std::size_t received_size;
	    if ((old_state.greeting[0] >> 5) != 0)
	    {
		received_size = 0;
	    }
	    else
	    {
		std::memcpy(new_state.frame->buffer,
			    old_state.greeting, sizeof(old_state.greeting));
		received_size = sizeof(old_state.greeting);
	    }
	    return receive_state(new_state.frame->buffer + received_size,
				 DIF_SEQUENCE_SIZE - received_size,
				 &connection::handle_source_sequence);
	}
    }
    // Sinks should send 'SINK' as a greeting (and then nothing else).
    else if (std::memcmp(old_state.greeting, GREETING_SINK, GREETING_SIZE) == 0)
    {
	conn_state_ = sink_state();
	sink_state & new_state = boost::get<sink_state>(conn_state_);
	new_state.frame_pos = 0;
	new_state.overflowed = false;
	new_state.sink_id = server_.mixer_.add_sink(this);

	static uint8_t dummy;
	return receive_state(&dummy,
			     1,
			     &connection::handle_unexpected_input);
    }

    return receive_state();
}

server::connection::receive_state server::connection::handle_source_sequence()
{
    source_state & state = boost::get<source_state>(conn_state_);

    if (dv_parse_header(state.decoder, state.frame->buffer) >= 0)
    {
	state.frame->system = state.decoder->system;
	state.frame->size = state.decoder->frame_size;
	return receive_state(state.frame->buffer + DIF_SEQUENCE_SIZE,
			     state.frame->size - DIF_SEQUENCE_SIZE,
			     &connection::handle_source_frame);
    }

    return receive_state();
}

server::connection::receive_state server::connection::handle_source_frame()
{
    source_state & state = boost::get<source_state>(conn_state_);

    server_.mixer_.put_frame(state.source_id, state.frame);
    state.frame.reset();
    state.frame = server_.mixer_.allocate_frame();

    return receive_state(state.frame->buffer,
			 DIF_SEQUENCE_SIZE,
			 &connection::handle_source_sequence);
}

server::connection::receive_state server::connection::handle_unexpected_input()
{
    return receive_state();
}

void server::connection::put_frame(const mixer::frame_ptr & frame)
{
    sink_state & state = boost::get<sink_state>(conn_state_);

    bool was_empty = false;
    {
	boost::mutex::scoped_lock lock(sink_queue_mutex_);
	if (state.frames.full())
	{
	    state.overflowed = true;
	}
	else
	{
	    if (state.frames.empty())
		was_empty = true;
	    state.frames.push(frame);
	}
    }
    if (was_empty)
	server_.enable_output_polling(socket_);
}
