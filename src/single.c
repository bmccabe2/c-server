/* single.c: Single User HTTP Server */

#include "main.h"

#include <errno.h>
#include <string.h>

#include <unistd.h>

/**
 * Handle one HTTP request at a time.
 *
 * @param 	sfd	Server socket file descriptor.
 * @return	Exit status of server (EXIT_SUCCESS).
 **/
int single_server(int sfd) {
	/* Accept and handle HTTP request */
	while (true) {
		/* Accept request */
		Request *r = accept_request(sfd);
		if(!r) {
			continue;
		}
		/* Handle request */
		handle_request(r);

		/* free request */
		free_request(r);
	}

	return EXIT_SUCCESS;
}


/* vim: set expandtab sts=4 wt=4 ts=8 ft=c: */
