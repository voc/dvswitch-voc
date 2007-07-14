/* Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
 * See the file "COPYING" for licence details.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <unistd.h>

#include "config.h"
#include "protocol.h"
#include "socket.h"

static struct option options[] = {
    {"card",   1, NULL, 'c'},
    {"host",   1, NULL, 'h'},
    {"port",   1, NULL, 'p'},
    {"help",   0, NULL, 'H'},
    {NULL,     0, NULL, 0}
};

static char * firewire_card = NULL;
static char * mixer_host = NULL;
static char * mixer_port = NULL;

static void handle_config(const char * name, const char * value)
{
    if (strcmp(name, "FIREWIRE_CARD") == 0)
    {
	free(firewire_card);
	firewire_card = strdup(value);
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
Usage: %s [{-c|--card} FIREWIRE-CARD \\\n\
           [{-h|--host} MIXER-HOST] [{-p|--port} MIXER-PORT]\n",
	    progname);
}

int main(int argc, char ** argv)
{
    /* Initialise settings from configuration files. */
    dvswitch_read_config(handle_config);

    /* Parse arguments. */

    int opt;
    while ((opt = getopt_long(argc, argv, "c:h:p:", options, NULL)) != -1)
    {
	switch (opt)
	{
	case 'c':
	    free(firewire_card);
	    firewire_card = strdup(optarg);
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

    if (firewire_card)
	printf("INFO: Reading from Firewire card %s\n", firewire_card);
    else
	printf("INFO: Reading from first Firewire card with camera\n");
    printf("INFO: Connecting to %s:%s\n", mixer_host, mixer_port);
    int sock = create_connected_socket(mixer_host, mixer_port);
    assert(sock >= 0); /* create_connected_socket() should handle errors */
    if (write(sock, GREETING_SOURCE, GREETING_SIZE) != GREETING_SIZE)
    {
	perror("ERROR: write");
	exit(1);
    }
    if (dup2(sock, STDOUT_FILENO) < 0)
    {
	perror("ERROR: dup2");
	return 1;
    }
    close(sock);
    if (firewire_card)
	execlp("dvgrab",
	       "dvgrab", "--card", firewire_card, "--noavc", "-", NULL);
    else
	execlp("dvgrab",
	       "dvgrab", "--noavc", "-", NULL);
    perror("ERROR: execvp");
    return 1;
}
