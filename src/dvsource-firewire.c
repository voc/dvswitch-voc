#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <unistd.h>

#include "config.h"
#include "socket.h"

static struct option options[] = {
    {"device", 1, NULL, 'd'},
    {"host",   1, NULL, 'h'},
    {"port",   1, NULL, 'p'},
    {"help",   0, NULL, 'H'},
    {NULL,     0, NULL, 0}
};

static char * firewire_device = NULL;
static char * mixer_host = NULL;
static char * mixer_port = NULL;

static void handle_config(const char * name, const char * value)
{
    if (strcmp(name, "FIREWIRE_DEVICE") == 0)
    {
	free(firewire_device);
	firewire_device = strdup(value);
    }
    else if (strcmp(name, "MIXER_HOST") == 0)
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
Usage: %s [{-d|--device} FIREWIRE-DEVICE \\\n\
           [{-h|--host} MIXER-HOST] [{-p|--port} MIXER-PORT]\n",
	    progname);
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
	case 'd':
	    free(firewire_device);
	    firewire_device = strdup(optarg);
	    break;
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

    if (optind != argc)
    {
	fprintf(stderr, "%s: excess argument \"%s\"\n",
		argv[0], argv[optind]);
	usage(argv[0]);
	return 2;
    }

    /* Connect to the mixer, set that as stdout, and run dvgrab. */

    printf("INFO: Reading from %s\n",
	   firewire_device ? firewire_device : "/dev/raw1394");
    printf("INFO: Connecting to %s:%s\n", mixer_host, mixer_port);
    int sock = create_connected_socket(mixer_host, mixer_port);
    assert(sock >= 0); /* create_connected_socket() should handle errors */
    if (dup2(sock, STDOUT_FILENO) < 0)
    {
	perror("ERROR: dup2");
	return 1;
    }
    close(sock);
    if (firewire_device)
	execlp("dvgrab", "--dv1394", firewire_device, "--noavc", "-");
    else
	execlp("dvgrab", "--noavc", "-");
    perror("ERROR: execvp");
    return 1;
}
