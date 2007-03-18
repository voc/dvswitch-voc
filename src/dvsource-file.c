#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>

#include "config.h"
#include "socket.h"

static struct option options[] = {
    {"host",   1, NULL, 'h'},
    {"port",   1, NULL, 'p'},
    {"help",   0, NULL, 'H'},
    {NULL,     0, NULL, 0}
};

static char * mixer_host = NULL;
static char * mixer_port = NULL;

static void handle_config(const char * name, const char * value)
{
    if (strcmp(name, "MIXER_HOST") == 0)
    {
	free(mixer_host);
	mixer_host = strdup(value);
    }
    else if (strcmp(name, "MIXER_PORT") == 0)
    {
	free(mixer_port);
	mixer_port = strdup(value);
    }
}

static void usage(const char * progname)
{
    fprintf(stderr,
	    "\
Usage: %s [{-h|--host} MIXER-HOST] [{-p|--port} MIXER-PORT] FILE\n",
	    progname);
}

static void create_frame_timer(void)
{
    /*
     * On Linux, CLOCK_MONOTONIC matches the kernel interval timer
     * (resolution is controlled by HZ) and there is no
     * CLOCK_MONOTONIC_HR.
     */
    struct timespec res;
    if (clock_getres(CLOCK_MONOTONIC, &res) == -1)
    {
	perror("ERROR: clock_get_res");
	exit(1);
    }
    if (res.tv_sec != 0 || res.tv_nsec > 10000000)
    {	
	fprintf(stderr, 
		"ERROR: CLOCK_MONOTONIC resolution is too low (%lu.%09lds)\n",
		(unsigned long)res.tv_sec, res.tv_nsec);
	exit(1);
    }

    /* TODO: Er, create the timer. */
}

int main(int argc, char ** argv)
{
    /* Initialise settings from configuration files. */
    dvswitch_read_config(handle_config);

    /* Parse arguments. */

    int opt;
    while ((opt = getopt_long(argc, argv, "h:p:", options, NULL)) != -1)
    {
	switch (opt)
	{
	case 'h':
	    free(mixer_host);
	    mixer_host = strdup(optarg);
	    break;
	case 'p':
	    free(mixer_port);
	    mixer_port = strdup(optarg);
	    break;
	case 'H': /* --help */
	    usage(argv[0]);
	    return 0;
	default:
	    usage(argv[0]);
	    return 2;
	}
    }

    if (!mixer_host || !mixer_port)
    {
	fprintf(stderr, "%s: mixer hostname and port not defined\n",
		argv[0]);
	return 2;
    }

    if (optind != argc - 1)
    {
	if (optind == argc)
	{
	    fprintf(stderr, "%s: missing filename\n",
		    argv[0]);
	}
	else
	{
	    fprintf(stderr, "%s: excess argument \"%s\"\n",
		    argv[0], argv[optind + 1]);
	}
	usage(argv[0]);
	return 2;
    }

    const char * filename = argv[optind];

    /* Prepare to read the file and connect a socket to the mixer. */

    create_frame_timer();

    printf("INFO: Reading from %s\n", filename);
    int file = open(filename, O_RDONLY, 0);
    if (file < 0)
    {
	perror("ERROR: open");
	return 1;
    }
    printf("INFO: Connecting to %s:%s\n", mixer_host, mixer_port);
    int sock = create_connected_socket(mixer_host, mixer_port);
    assert(sock >= 0); /* create_connected_socket() should handle errors */

    /* TODO: Main loop. */
    close(sock);
    close(file);

    return 0;
}
