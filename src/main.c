/* main.c: Runs a Simple HTTP Server */

#include "main.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>

/*Global Variables */
char *Port		= "9898";
char *MimeTypesPath	= "/etc/mime.types";
char *DefaultMimeType	= "text/plain";
char *RootPath		= "www";

/**
 * Display usage message and exit with specified status code.
 *
 * @param 	progname	Program Name.
 * @param	status		Exit status.
 */
void usage(const char *progname, int status) {
	fprintf(stderr, "Usage: %s [hcmMpr]\n", progname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "	-h		Display help message\n");
	fprintf(stderr, "	-c mode		Single or Forking mode\n");
	fprintf(stderr, "	-m path		Path to mimetypes file\n");
	fprintf(stderr, "	-M mimetype	Default mimetype\n");
	fprintf(stderr, "	-p port		Port to listen on\n");
	fprintf(stderr, "	-M path 	Root directory\n");
	exit(status);
}

/**
 * Parse command-line options.
 *
 * @param	argc	Number of arguments.
 * @param	argv	Array of argument strings.
 * @param	mode	Pointer to ServerMode variable.
 * @return true if parsing was succesful, false if there was an error.
 *
 * This should set the mode, MimeTypePath, DefaultMimeType, Port, and RootPath
 * if specified.
 */
bool parse_options(int argc, char *argv[], ServerMode *mode) {
	int argind = 1;
	while (argind < argc && strlen(argv[argind]) > 1 && argv[argind][0] == '-') {
		char *arg = argv[argind++];
		switch(arg[1]) {
			case 'c':
				if (streq(argv[argind], "single")) {
					*mode = SINGLE;
				} else if (streq(argv[argind], "forking")) {
					*mode = FORKING;
				} else {
					return false;
				}
				argind++;
				break;
			case 'h':
				usage(argv[0], EXIT_SUCCESS);
				break;
			case 'm':
				MimeTypesPath =argv[argind++];
				break;
			case 'M':
				DefaultMimeType = argv[argind++];
				break;
			case 'p':
				Port = argv[argind++];
				break;
			case 'r':
				RootPath = argv[argind++];
				break;
			default:
				return false;
				break;
		}
	}
	return true;
}

/**
 * Parses command line options and starts appropriate server
 **/

int main(int argc, char *argv[]) {
	ServerMode mode = SINGLE;
	int status = EXIT_SUCCESS;

	/* Parse command line options */
	if (!parse_options(argc, argv, &mode)) {
		usage(argv[0],EXIT_FAILURE);
	}

	/* listen to server socket */
	int socket_fd = socket_listen(Port);
	if (socket_fd < 0) {
		fprintf(stderr, "socket_listen failed\n");
		return EXIT_FAILURE;
	}
	
	/* Determine the real RootPath */
	char root_path_buffer[BUFSIZ];
	RootPath = realpath(RootPath, root_path_buffer);

	log("Listening on port %s", Port);
	debug("RootPath 	= %s", RootPath);
	debug("MimeTypePath 	= %s", MimeTypesPath);
	debug("DefaultMimeType 	= %s", DefaultMimeType);
	debug("ConcurrencyMode 	= %s", mode == SINGLE ? "Single" : "Forking");

	if (mode == SINGLE) {
		single_server(socket_fd);
	} else {
		forking_server(socket_fd);
	}
	return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c */
