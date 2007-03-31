// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <ostream>
#include <string>

#include <getopt.h>

#include <gtkmm/main.h>
#include <gtkmm/window.h>

#include "config.h"
#include "dv_view_widget.hpp"
#include "dv_selector_widget.hpp"

namespace
{
    struct option options[] = {
	{"host",             1, NULL, 'h'},
	{"port",             1, NULL, 'p'},
	{"output-directory", 1, NULL, 'o'},
	{"help",             0, NULL, 'H'},
	{NULL,               0, NULL, 0}
    };

    std::string mixer_host;
    std::string mixer_port;
    std::string output_directory;

    extern "C"
    {
	void handle_config(const char * name, const char * value)
	{
	    if (std::strcmp(name, "MIXER_HOST") == 0)
		mixer_host = value;
	    else if (strcmp(name, "MIXER_PORT") == 0)
		mixer_port = value;
	    else if (strcmp(name, "OUTPUT_DIRECTORY") == 0)
		output_directory = value;
	}
    }

    void usage(const char * progname)
    {
	std::cerr << "\
Usage: " << progname << " [gtk-options] \\\n\
           [{-h|--host} LISTEN-HOST] [{-p|--port} LISTEN-PORT]\n";
    }
}

int main(int argc, char **argv)
{
    try
    {
	dvswitch_read_config(handle_config);

	// Initialise Gtk
	Gtk::Main kit(argc, argv);

	// Complete option parsing with Gtk's options out of the way.

	int opt;
	while ((opt = getopt_long(argc, argv, "h:p:", options, NULL)) != -1)
	{
	    switch (opt)
	    {
	    case 'h':
		mixer_host = optarg;
		break;
	    case 'p':
		mixer_port = optarg;
		break;
	    case 'H': /* --help */
		usage(argv[0]);
		return 0;
	    default:
		usage(argv[0]);
		return 2;
	    }
	}

	if (mixer_host.empty() || mixer_port.empty())
	{
	    std::cerr << argv[0] << ": mixer hostname and port not defined\n";
	    return 2;
	}

	// TODO: Listen on port, create window, set up frame processing
	// thread... in fact the whole program.
    }
    catch (std::exception & e)
    {
	std::cerr << "ERROR: " << e.what() << "\n";
	return EXIT_FAILURE;
    }
}
