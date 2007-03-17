#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

int connected_socket(const char * host, const char * port)
{
    struct addrinfo mixer_addr_hints = {
	.ai_family =   AF_UNSPEC,
	.ai_socktype = SOCK_STREAM,
	.ai_flags =    AI_ADDRCONFIG
    };
    struct addrinfo * mixer_addr;
    int error;
    if ((error = getaddrinfo(host, port, &mixer_addr_hints, &mixer_addr)))
    {
	fprintf(stderr, "ERROR: getaddrinfo: %s\n", gai_strerror(error));
	exit(1);
    }

    /* XXX Should we walk the list rather than only trying the first? */
    int sock = socket(mixer_addr->ai_family,
		      mixer_addr->ai_socktype,
		      mixer_addr->ai_protocol);
    if (sock < 0)
    {
	perror("ERROR: socket");
	exit(1);
    }
    if (connect(sock, mixer_addr->ai_addr, mixer_addr->ai_addrlen) != 0)
    {
	perror("ERROR: connect");
	exit(1);
    }

    freeaddrinfo(mixer_addr);
    return sock;
}
