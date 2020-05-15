/* forking.c: Forking HTTP Server */

#include "main.h"

#include <errno.h>
#include <signal.h>
#include <string.h>

#include <unistd.h>

/**
 * Fork incoming HTTP Requests to handle them concurrently
 *
 * @param	sfd	Server socket file descriptor.
 * @return 	Exit status of the server (EXIT_SUCCESS).
 *
 * The parent should accept a request and then fork off and let the child
 * handle the request.
 **/
int forking_server(int sfd) {
	/* Accept and handle HTTP request */
	while (true) {
		/* Accept Request */
		Request *r = accept_request(sfd);
		
		/* Ignore children */
		signal(SIGCHLD,SIG_IGN);

		/* Fork off child process to handle the request */
		pid_t pid = fork();
		if (pid < 0) {
			free_request(r);
			continue;
		}
		if (pid == 0) {
			handle_request(r);
			exit(EXIT_SUCCESS);
		}
		else {
			free_request(r);
		}
	}
	/* close server socket */
	return EXIT_SUCCESS;
}


/* vim: expandtab sts=4 sw=4 ts=8 ft=c */
