/* Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
 * See the file "COPYING" for licence details.
 */

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>

#include <libdv/dv.h>

#include "config.h"
#include "dif.h"
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

struct transfer_params {
    timer_t        timer;
    dv_decoder_t * decoder;
    int            file;
    int            sock;
};

static timer_t create_frame_timer(void)
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
    if (res.tv_sec != 0 || res.tv_nsec > frame_time_ns_525_60)
	fprintf(stderr, 
		"WARNING: CLOCK_MONOTONIC resolution is too low"
		" (%lu.%09lus)\n",
		(unsigned long)res.tv_sec, (unsigned long)res.tv_nsec);

    struct sigevent event = {
	.sigev_notify = SIGEV_SIGNAL,
	.sigev_signo =  SIGALRM
    };
    timer_t timer;
    if (timer_create(CLOCK_MONOTONIC, &event, &timer) != 0)
    {
	perror("ERROR: timer_create");
	exit(1);
    }
    return timer;
}

static void transfer_frames(struct transfer_params * params)
{
    /* Assume the timer will signal SIGALRM; block it so we can handle
     * it synchronously.
     */
    sigset_t sigset_alarm;
    sigemptyset(&sigset_alarm);
    sigaddset(&sigset_alarm, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &sigset_alarm, NULL) != 0)
    {
	perror("ERROR: sigprocmask");
	exit(1);
    }

    dv_system_t last_frame_system = e_dv_system_none;
    static uint8_t buf[dif_block_size * frame_blocks_max];
    ssize_t size;

    while ((size = read(params->file, buf, dif_pack_size))
	   == (ssize_t)dif_pack_size)
    {
	if (dv_parse_header(params->decoder, buf) < 0)
	{
	    fprintf(stderr, "ERROR: dv_parse_header failed\n");
	    exit(1);
	}

	/* (Re)set the timer according to this frame's video system. */
	if (params->decoder->system != last_frame_system)
	{
	    last_frame_system = params->decoder->system;
	    struct itimerspec interval;
	    interval.it_interval.tv_sec = 0;
	    if (params->decoder->system == e_dv_system_525_60)
		interval.it_interval.tv_nsec = frame_time_ns_525_60;
	    else
		interval.it_interval.tv_nsec = frame_time_ns_625_50;
	    interval.it_value = interval.it_interval;
	    if (timer_settime(params->timer, 0, &interval, NULL) != 0)
	    {
		perror("ERROR: timer_settime");
		exit(1);
	    }
	}

	if (read(params->file, buf + dif_pack_size,
		 params->decoder->frame_size - dif_pack_size)
	    != (ssize_t)(params->decoder->frame_size - dif_pack_size))
	{
	    perror("ERROR: read");
	    exit(1);
	}
	if (write(params->sock, buf, params->decoder->frame_size)
	    != (ssize_t)params->decoder->frame_size)
	{
	    perror("ERROR: write");
	    exit(1);
	}

	/* Wait for next frame time. */
	int dummy;
	sigwait(&sigset_alarm, &dummy);
    }

    if (size != 0)
    {
	perror("ERROR: read");
	exit(1);
    }
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

    struct transfer_params params;
    params.timer = create_frame_timer();
    dv_init(TRUE, TRUE);
    params.decoder = dv_decoder_new(0, TRUE, TRUE);
    if (!params.decoder)
    {
	fprintf(stderr, "ERROR: dv_decoder_new failed\n");
	exit(1);
    }
    printf("INFO: Reading from %s\n", filename);
    params.file = open(filename, O_RDONLY, 0);
    if (params.file < 0)
    {
	perror("ERROR: open");
	return 1;
    }
    printf("INFO: Connecting to %s:%s\n", mixer_host, mixer_port);
    params.sock = create_connected_socket(mixer_host, mixer_port);
    assert(params.sock >= 0); /* create_connected_socket() should handle errors */

    transfer_frames(&params);

    close(params.sock);
    close(params.file);
    dv_decoder_free(params.decoder);
    dv_cleanup();

    return 0;
}
