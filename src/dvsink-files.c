// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "dif.h"
#include "protocol.h"
#include "socket.h"

static struct option options[] = {
    {"host",   1, NULL, 'h'},
    {"port",   1, NULL, 'p'},
    {"help",   0, NULL, 'H'},
    {NULL,     0, NULL, 0}
};

static char * mixer_host = NULL;
static char * mixer_port = NULL;
static char * output_name_format = NULL;

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
    else if (strcmp(name, "OUTPUT_NAME_FORMAT") == 0)
    {
	free(output_name_format);
	output_name_format = strdup(value);
    }
}

static void usage(const char * progname)
{
    fprintf(stderr,
	    "\
Usage: %s [{-h|--host} MIXER-HOST] [{-p|--port} MIXER-PORT] [NAME-FORMAT]\n",
	    progname);
}

struct transfer_params {
    int            sock;
};

static void transfer_frames(struct transfer_params * params)
{
    static uint8_t buf[SINK_FRAME_HEADER_SIZE + DIF_MAX_FRAME_SIZE];
    const struct dv_system * system;

    time_t now;
    struct tm now_local;
    size_t name_buf_len;
    char * name_buf;

    // Work out how long the output file name could be.  Tell
    // strftime() half of the buffer length so we have space for
    // expanding local names and a suffix if this doesn't make
    // sufficiently unique names.
    now = time(0);
    localtime_r(&now, &now_local);
    for (name_buf = 0, name_buf_len = 200;
	 ((name_buf = realloc(name_buf, name_buf_len))
	  && (strftime(name_buf, name_buf_len / 2, output_name_format,
		       &now_local)
	      == 0));
	 name_buf_len *= 2)
	;
    if (!name_buf)
    {
	perror("realloc");
	exit(1);
    }

    int file = -1;
    ssize_t read_size;

    for (;;)
    {
	size_t wanted_size = SINK_FRAME_HEADER_SIZE;
	size_t buf_pos = 0;
	do
	{
	    read_size = read(params->sock, buf + buf_pos,
			     wanted_size - buf_pos);
	    if (read_size <= 0)
		goto read_failed;
	    buf_pos += read_size;
	}
	while (buf_pos != wanted_size);

	// Open/close files as necessary
	if (buf[SINK_FRAME_CUT_FLAG_POS] || file < 0)
	{
	    if (file >= 0)
	    {
		close(file);
		file = -1;
	    }

	    // Check for stop indicator
	    if (buf[SINK_FRAME_CUT_FLAG_POS] == 'S')
	    {
		printf("INFO: Stopped recording\n");
		fflush(stdout);
		continue;
	    }

	    now = time(0);
	    localtime_r(&now, &now_local);
	    size_t name_len =
		strftime(name_buf, name_buf_len, output_name_format,
			 &now_local);
	    assert(name_len != 0);

	    // Add ".dv" extension if missing.  Add distinguishing
	    // number before it if necessary to avoid collision.
	    // XXX This code is disgusting.
	    int suffix_num = 0;
	    if (name_len <= 3 || strcmp(name_buf + name_len - 3, ".dv") != 0)
		strcpy(name_buf + name_len, ".dv");
	    else
		name_len -= 3;
	    while ((file = open(name_buf, O_CREAT | O_EXCL | O_WRONLY, 0666))
		   < 0
		   && errno == EEXIST)
		sprintf(name_buf + name_len, "-%d.dv", ++suffix_num);
	    if (file < 0)
	    {
		perror("open");
		exit(1);
	    }
	    printf("INFO: Created file %s\n", name_buf);
	    fflush(stdout);
	}

	wanted_size = SINK_FRAME_HEADER_SIZE + DIF_SEQUENCE_SIZE;
	do
	{
	    read_size = read(params->sock, buf + buf_pos,
			     wanted_size - buf_pos);
	    if (read_size <= 0)
		goto read_failed;
	    buf_pos += read_size;
	}
	while (buf_pos != wanted_size);

	system = dv_buffer_system(buf + SINK_FRAME_HEADER_SIZE);
	wanted_size = SINK_FRAME_HEADER_SIZE + system->size;
	do
	{
	    read_size = read(params->sock, buf + buf_pos,
			     wanted_size - buf_pos);
	    if (read_size <= 0)
		goto read_failed;
	    buf_pos += read_size;
	}
	while (buf_pos != wanted_size);

	if (write(file, buf + SINK_FRAME_HEADER_SIZE, system->size)
	    != (ssize_t)system->size)
	{
	    perror("ERROR: write");
	    exit(1);
	}
    }

read_failed:
    if (read_size != 0)
    {
	perror("ERROR: read");
	exit(1);
    }

    if (file >= 0)
	close(file);
}

int main(int argc, char ** argv)
{
    // Initialise settings from configuration files.
    dvswitch_read_config(handle_config);

    // Parse arguments.

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
	case 'H': // --help
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

    if (optind < argc)
	output_name_format = argv[optind];

    if (optind < argc - 1)
    {
	fprintf(stderr, "%s: excess argument \"%s\"\n",
		argv[0], argv[optind + 1]);
	usage(argv[0]);
	return 2;
    }

    if (!output_name_format || !output_name_format[0])
    {
	fprintf(stderr, "%s: output name format not defined or empty\n",
		argv[0]);
	return 2;
    }

    struct transfer_params params;
    printf("INFO: Connecting to %s:%s\n", mixer_host, mixer_port);
    fflush(stdout);
    params.sock = create_connected_socket(mixer_host, mixer_port);
    assert(params.sock >= 0); // create_connected_socket() should handle errors
    if (write(params.sock, GREETING_REC_SINK, GREETING_SIZE) != GREETING_SIZE)
    {
	perror("ERROR: write");
	exit(1);
    }

    transfer_frames(&params);

    close(params.sock);

    return 0;
}
