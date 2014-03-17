// Copyright 2007-2008 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <ostream>
#include <string>

#include <getopt.h>

#include <gtkmm/main.h>
#include <sigc++/functors/slot.h>

#include "avcodec_wrap.h"

#include "config.h"
#include "mixer.hpp"
#include "mixer_window.hpp"
#include "server.hpp"

bool expert_mode = false;

namespace
{
    struct option options[] = {
	{"host",             1, NULL, 'h'},
	{"port",             1, NULL, 'p'},
	{"help",             0, NULL, 'H'},
	{"expert",           0, NULL, 'e'},
	{"pip",              0, NULL, 'i'},
	{NULL,               0, NULL, 0}
    };

    std::string mixer_host;
    std::string mixer_port;

    extern "C"
    {
	void handle_config(const char * name, const char * value)
	{
	    if (std::strcmp(name, "MIXER_HOST") == 0)
		mixer_host = value;
	    else if (strcmp(name, "MIXER_PORT") == 0)
		mixer_port = value;
	}
    }

    void usage(const char * progname)
    {
	std::cerr << "\
Usage: " << progname << " [gtk-options] \\\n\
           [{-h|--host} LISTEN-HOST] [{-p|--port} LISTEN-PORT] [{-e|--expert}] [{-i|--pip} 10,10,210,160]\n";
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

	rectangle pip_area_;
	pip_area_.left = 10;
	pip_area_.top = 10;
	pip_area_.right = 10 + 200;
	pip_area_.bottom = 10 + 150;

	int opt;
	while ((opt = getopt_long(argc, argv, "h:p:i:", options, NULL)) != -1)
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
	    case 'e':
		expert_mode = true;
		break;
	    case 'i':
			int left, top, width, height;
			sscanf(optarg, "%i,%i,%i,%i", &left, &top, &width, &height);

			pip_area_.left = left;
			pip_area_.top = top;
			pip_area_.right = left + width;
			pip_area_.bottom = top + height;
		break;
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

	// The mixer must be created before the window, since we pass
	// a reference to the mixer into the window's constructor to
	// allow it to adjust the mixer's controls.
	// However, the mixer must also be destroyed before the
	// window, since as long as it exists it may call into the
	// window as a monitor.
	// This should probably be fixed by a smarter design, but for
	// now we arrange this by attaching the window to an auto_ptr.
	std::auto_ptr<mixer_window> the_window;
	mixer the_mixer;
	server the_server(mixer_host, mixer_port, the_mixer);
	the_window.reset(new mixer_window(the_mixer, pip_area_));
	the_mixer.set_monitor(the_window.get());
	the_window->show();
	the_window->signal_hide().connect(sigc::ptr_fun(&Gtk::Main::quit));
	Gtk::Main::run();
	return EXIT_SUCCESS;
    }
    catch (std::exception & e)
    {
	std::cerr << "ERROR: " << e.what() << "\n";
	return EXIT_FAILURE;
    }
    catch (Glib::Exception & e)
    {
       std::cerr << "ERROR: " << e.what() << "\n";
       return EXIT_FAILURE;
    }
}
