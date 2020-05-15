/* request.c: HTTP Request Functions */

#include "main.h"

#include <errno.h>
#include <string.h>

#include <unistd.h>

int parse_request_method(Request *r);
int parse_request_headers(Request *r);

/**
 * Accept request from server socket.
 *
 * @param	sfd	Server socket file descriptor
 * @return 	Newly allocated Request structure.
 *
 * This function does the following
 *
 * 1. Allocates a request struct initialized to 0.
 * 2. Initializes the headers list in the request struct.
 * 3. Accepts a client connection from the server socket.
 * 4. Looks up the client information and stores it in the request struct.
 * 5. Opens the client socket stream for the request struct.
 * 6. Returns the request struct.
 *
 * The returned request struct must be deallocated using free_request.
 **/
Request * accept_request(int sfd) {
	Request *r;
	struct sockaddr raddr;
	socklen_t rlen = sizeof(struct sockaddr);

	/* Allocate request struct (zeroed) */
	r = calloc(1, sizeof(Request));
	if (!r) {
		log("Unable to calloc: %s", strerror(errno));
		return NULL;
	}

	int client_fd = accept(sfd, &raddr, &rlen);
	if (client_fd < 0){
		log("Unable to accept: %s", strerror(errno));
		goto fail;
	}
	r->fd = client_fd;

	/* Lookup client information */
	int e = getnameinfo(&raddr, rlen, r->host, NI_MAXHOST, r->port, NI_MAXSERV, 0);
	if (e != 0) {
		log("Unable to getnameinfo: %s", gai_strerror(e));
		goto fail;
	}

	/* Open Socket Stream */
	FILE *client_file = fdopen(client_fd, "w+");
	if (!client_file) {
		log("Unable to fdopen: %s",strerror(errno));
		close(client_fd);
		goto fail;
	}
	r->file = client_file;

	log("Accepted request from %s:%s",r->host,r->port);
	return r;

fail:
	/* Deallocate request struct */
	free_request(r);
	return NULL;
}


/**
 * Deallocate request struct.
 *
 * @param 	r	Request structure.
 *
 * This function does the following
 *
 * 1. Closes the request socket stream or file descriptor.
 * 2. Frees all allocated strings in request struct.
 * 3. Frees all of the headers (including any allocated fields).
 * 4. Frees request struct.
 **/
void free_request(Request *r) {
	log("Freeing request...");
	if (!r) {
		return;
	}

	/* Close socket file or fd */
	fclose(r->file);

	/*Free alloacted strings */
	free(r->method);
	free(r->uri);
	free(r->path);
	free(r->query);

	
	/* Free headers */
	struct header *temp;
	while(r->headers) {
		temp = r->headers->next;
		free(r->headers->name);
		free(r->headers->value);
		free(r->headers);
		r->headers = temp;
	}

	/* Free request */
	free(r);
}

/**
 * Parse HTTP Request.
 *
 * @param	r	Request structure.
 * @return	-1 on error and 0 on successs.
 *
 * This function first parses the request method, any query, and then the 
 * headers, returning 0 on success and -1 on error.
 **/
int parse_request(Request *r) {
	log("Parsing request...");
	/* Parse HTTP Request Method */
	if(parse_request_method(r) != 0)
		return -1;
	if(parse_request_headers(r) != 0)
		return -1;
	return 0;
}

/**
 * Parse HTTP Request Method and URI.
 *
 * @param	r	Request structure.
 * @return 	-1 on error and 0 on success.
 *
 * HTTP Requests come in the form
 *
 * <METHOD> <URI>[QUERY] HTTP/<VERSION>
 *
 * E.G.
 *
 *   GET / HTTP/1.1
 *   GET /cgi.script?q=foo HTTP/1.0
 *
 * This function extracts the method, uri, and query (if it exists).
 **/
int parse_request_method(Request *r) {
	char buffer[BUFSIZ];
	char *method;
	char *uri;
	char *query = "";

	/* Read line from socket */
	if(!fgets(buffer,BUFSIZ,r->file)) {
		log("Failed to read line from socket");
		goto fail;
	}

	/* Parse method and uri */
	method 	= strtok(buffer, " \t\n");
	uri	= strtok(NULL, " \t\n");

	if (!method || !uri)
		goto fail;
	
	/* Parse query from uri */
	if (strchr(uri, '?')) {
		query = strchr(uri,'?') + 1;
		query = strtok(query, "# \n\t");
		uri = strtok(uri, "?");
	}

	/* record method, uri and query in request struct */
	r->method = strdup(method);
	r->uri = strdup(uri);
	r->query = strdup(query);

	return 0;

fail:
	return -1;
}

/**
 * Parse HTTP Request Headers.
 *
 * @param	r
 * @return	-1 on error and 0 on success.
 *
 * HTTP Headers come in the form:
 *
 *   <NAME>: <VALUE>
 *
 * Example:
 *
 *   Host: localhost:8888
 *   User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:29.0) Gecko/20100101 Firefox/29.0
 *   Accept: text/html,application/xhtml+xml
 *   Accept-Language: en-US,en;q=0.5
 *   Accept-Encoding: gzip, deflate
 *   Connection: keep-alive
 *
 * This function parses the stream from the request socket using the following
 * pseudo-code:
 *   while (buffer = read_from_socket() and buffer is not empty:
 *       name, value 	= buffer.split(':')
 *       header		= new Header(name, value)
 *       headers.append(header)
 **/
 int parse_request_headers(Request *r) {
	 struct header *curr = NULL;
	 char buffer[BUFSIZ];
	 char *value;


	/* Parse headers from socket */
	while(fgets(buffer,BUFSIZ,r->file) && strlen(buffer) > 2) {
	       	chomp(buffer);
		struct header *new = malloc(sizeof(struct header));
		char *colon = strchr(buffer, ':');
		if (!colon) {
			free(new);
			goto fail;
		}
		new->name = strndup(buffer, colon - buffer);
		value = skip_whitespace(colon + 1); // + 1 to skip over ':'
		new->value = strdup(value);
		new->next = curr;
		curr = new;
	}

	r->headers = curr;

#ifndef NDEBUG
	for (struct header *header = r->headers; header; header = header->next) {
		debug("HTTP HEADER %s = %s", header->name, header->value);
	}
#endif
	return 0;

fail:
	return -1;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c */
