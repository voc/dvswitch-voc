/* Copyright 2007-2009 Ben Hutchings.
 * See the file "COPYING" for licence details.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>

#include <asoundlib.h>

#include "config.h"
#include "dif.h"
#include "protocol.h"
#include "socket.h"

static struct option options[] = {
    {"host",   1, NULL, 'h'},
    {"port",   1, NULL, 'p'},
    {"system", 1, NULL, 's'},
    {"rate",   1, NULL, 'r'},
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
Usage: %s [{-h|--host} MIXER-HOST] [{-p|--port} MIXER-PORT] \\\
           [{-s|--system} ntsc|pal] [{-r|--rate} 48000|32000|44100] [DEVICE]\n",
	    progname);
}

struct transfer_params {
    snd_pcm_t *              pcm;
    const struct dv_system * system;
    enum dv_sample_rate      sample_rate_code;
    int                      sock;
};

static void dv_buffer_fill_dummy(uint8_t * buf, const struct dv_system * system)
{
    unsigned seq_num, block_num;
    uint8_t * block = buf;

    for (seq_num = 0; seq_num != system->seq_count; ++seq_num)
    {
	for (block_num = 0; block_num != DIF_BLOCKS_PER_SEQUENCE; ++block_num)
	{
	    uint8_t type, typed_block_num;

	    // Set block id
	    if (block_num == 0) // header
	    {
		type = 0x1f;
		typed_block_num = 0;
	    }
	    else if (block_num < 3) // subcode
	    {
		type = 0x3f;
		typed_block_num = block_num - 1;
	    }
	    else if (block_num < 6) // VAUX
	    {
		type = 0x56;
		typed_block_num = block_num - 3;
	    }
	    else if (block_num % 16 == 6) // audio
	    {
		type = 0x76;
		typed_block_num = block_num / 16;
	    }
	    else // video
	    {
		type = 0x96;
		typed_block_num = (block_num - 7) - (block_num - 7) / 16;
	    }
	    block[0] = type;
	    block[1] = (seq_num << 4) | 7;
	    block[2] = typed_block_num;

	    // Clear rest of the block
	    memset(block + DIF_BLOCK_ID_SIZE,
		   0xff,
		   DIF_BLOCK_SIZE - DIF_BLOCK_ID_SIZE);

	    // Set system code
	    if (block_num == 0)
		block[DIF_BLOCK_ID_SIZE] = (system == &dv_system_625_50) ? 0xbf : 0x3f;

	    block += DIF_BLOCK_SIZE;
	}
    }
}

static void transfer_frames(struct transfer_params * params)
{
    static uint8_t buf[DIF_MAX_FRAME_SIZE];
    int16_t samples[2 * 2000];
    unsigned serial_num = 0;

    dv_buffer_fill_dummy(buf, params->system);

    for (;;)
    {
	unsigned sample_count = 
	    params->system->sample_counts[params->sample_rate_code].std_cycle[
		serial_num % params->system->sample_counts[params->sample_rate_code].std_cycle_len];

	snd_pcm_sframes_t rc = snd_pcm_readi(params->pcm, samples, sample_count);
	if (rc != (snd_pcm_sframes_t)sample_count)
	{
	    fprintf(stderr, "ERROR: snd_pcm_readi: %s\n",
		    (rc < 0) ? snd_strerror(rc) : "underrun");
	    exit(1);
	}

	dv_buffer_set_audio(buf, params->sample_rate_code, sample_count, samples);

	if (write(params->sock, buf, params->system->size)
	    != (ssize_t)params->system->size)
	{
	    perror("ERROR: write");
	    exit(1);
	}

	++serial_num;
    }
}

int main(int argc, char ** argv)
{
    /* Initialise settings from configuration files. */
    dvswitch_read_config(handle_config);

    struct transfer_params params;
    char * system_name = NULL;
    long sample_rate = 48000;

    /* Parse arguments. */

    int opt;
    while ((opt = getopt_long(argc, argv, "h:p:s:r:", options, NULL)) != -1)
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
	case 's':
	    free(system_name);
	    system_name = strdup(optarg);
	    break;
	case 'r':
	    sample_rate = strtol(optarg, NULL, 10);
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

    if (!system_name || !strcasecmp(system_name, "pal"))
    {
	params.system = &dv_system_625_50;
    }
    else if (!strcasecmp(system_name, "ntsc"))
    {
	params.system = &dv_system_525_60;
    }
    else
    {
	fprintf(stderr, "%s: invalid system name \"%s\"\n", argv[0], system_name);
	return 2;
    }

    if (sample_rate == 32000)
    {
	params.sample_rate_code = dv_sample_rate_32k;
    }
    else if (sample_rate == 44100)
    {
	params.sample_rate_code = dv_sample_rate_44k1;
    }
    else if (sample_rate == 48000)
    {
	params.sample_rate_code = dv_sample_rate_48k;
    }
    else
    {
	fprintf(stderr, "%s: invalid sample rate %ld\n", argv[0], sample_rate);
	return 2;
    }

    if (argc > optind + 1)
    {
	fprintf(stderr, "%s: excess argument \"%s\"\n",
		argv[0], argv[optind + 1]);
	usage(argv[0]);
	return 2;
    }


    const char * device = (argc == optind) ? "default" : argv[optind];
    int rc;

    /* Prepare to capture and connect a socket to the mixer. */

    printf("INFO: Capturing from %s\n", device);
    rc = snd_pcm_open(&params.pcm, device, SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0)
    {
	fprintf(stderr, "ERROR: snd_pcm_open: %s\n", snd_strerror(rc));
	return 1;
    }

    snd_pcm_hw_params_t * hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    rc = snd_pcm_hw_params_any(params.pcm, hw_params);
    if (rc < 0)
    {
	fprintf(stderr, "ERROR: snd_pcm_hw_params_any: %s\n", snd_strerror(rc));
	return 1;
    }
    rc = snd_pcm_hw_params_set_access(params.pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc >= 0)
	rc = snd_pcm_hw_params_set_format(params.pcm, hw_params, SND_PCM_FORMAT_S16);
    if (rc >= 0)
	snd_pcm_hw_params_set_channels(params.pcm, hw_params, 2);
    if (rc >= 0)
	snd_pcm_hw_params_set_rate_resample(params.pcm, hw_params, 1);
    if (rc >= 0)
	snd_pcm_hw_params_set_rate(params.pcm, hw_params, sample_rate, 0);
    if (rc >= 0)
	rc = snd_pcm_hw_params(params.pcm, hw_params);
    if (rc < 0)
    {
	fprintf(stderr, "ERROR: snd_pcm_hw_params: %s\n", snd_strerror(rc));
	return 1;
    }
    rc = snd_pcm_start(params.pcm);
    if (rc < 0)
    {
	fprintf(stderr, "ERROR: snd_pcm_start: %s\n", snd_strerror(rc));
	return 1;
    }

    printf("INFO: Connecting to %s:%s\n", mixer_host, mixer_port);
    params.sock = create_connected_socket(mixer_host, mixer_port);
    assert(params.sock >= 0); /* create_connected_socket() should handle errors */
    if (write(params.sock, GREETING_SOURCE, GREETING_SIZE) != GREETING_SIZE)
    {
	perror("ERROR: write");
	exit(1);
    }

    transfer_frames(&params);

    close(params.sock);
    snd_pcm_close(params.pcm);

    return 0;
}
