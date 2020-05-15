/* handler.c: HTTP Request Handlers */

#include "main.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* Internal Declarations */
Status handle_browse_request(Request *request);
Status handle_file_request(Request *request);
Status handle_cgi_request(Request *request);
Status handle_error(Request *request, Status status);

/**
 * Handle HTTP Request.
 *
 * @param	r		HTTP request structure.
 * @return	Status of the HTTP request.
 *
 * This parses a request, determines the request path, determines the request type,
 * and then dispatches to the appropriate handler type.
 *
 * On error, handle_error should be used with an appropriate HTTP status code.
 **/
Status handle_request(Request *r){
	Status result;

	/* Parse request */
	if (parse_request(r) < 0) {
		return handle_error(r, HTTP_STATUS_BAD_REQUEST);
	}

	/* Determine request path */
	r->path = determine_request_path(r->uri);
	debug("HTTP REQUEST PATH: %s", r->path);
	if(!r->path) {
		return handle_error(r, HTTP_STATUS_NOT_FOUND);
	}

	/* Dispatch to appropriate request handler type based on file type */
	struct stat sb;
	if (stat(r->path, &sb) == -1) {
		log("Unable to stat %s", strerror(errno));
		return handle_error(r, HTTP_STATUS_INTERNAL_SERVER_ERROR);
	}

	if(S_ISDIR(sb.st_mode)) {			// if file is DIR
		log("Handling browse request...");
		result = handle_browse_request(r);	// Handle DIR
	} else if (S_ISREG(sb.st_mode)) {		// if file is FILE
		if (access(r->path, X_OK) == 0) { 	// if file is executable
			log("Handling CGI request...");
			result = handle_cgi_request(r);	// handle cgi
		}
		else if (sb.st_mode & S_IRUSR) {	// if readable
			log("Handling file request...");
			result = handle_file_request(r);
		} else {
			result = handle_error(r, HTTP_STATUS_INTERNAL_SERVER_ERROR);
		}
	} else {
		result = handle_error(r, HTTP_STATUS_INTERNAL_SERVER_ERROR);
	}

	// inode in stat structure say whether file is executable readable etc..
	
	log("HTTP REQUEST STATUS: %s", http_status_string(result));
	
	return result;
}


/**
 * Filter out current directory "." from scandir
 *
 * @param	dir	The directory to be filtered
 * return 	0 if the current directory, 1 if not.
 **/
int filter_curdir(const struct dirent *dir) {
	return streq(dir->d_name, ".") ? 0 : 1;
}

/**
 * Handle browse request.
 *
 * @param	r	HTTP Request structure.
 * @return	Status of the HTTP browse request.
 *
 * This lists the contents of a directory in HTML
 *
 * If the path cannot be opened or scanned as a directory, then handle error
 * with HTTP_STATUS_NOT_FOUND.
 **/
Status 	handle_browse_request(Request *r) {
	struct dirent **entries;
	int n;

	/* Open a directory for reading or scanning */
	DIR *d;
	d = opendir(r->path);
	if(!d) {
		log("Unable to opendir: %s\n", strerror(errno));
		return handle_error(r, HTTP_STATUS_NOT_FOUND);
	}
	
	/* Write HTTP header with OK status an text/html Content-type */
	fputs("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n",r->file);

	/* For each entry in directory emit HTML list item */
	n = scandir(r->path, &entries, filter_curdir, alphasort);
	if (n == -1) {
		log("Unable to scandir: %s\n", strerror(errno));
		closedir(d);
		return handle_error(r, HTTP_STATUS_NOT_FOUND);
	}

	/* if the directory already has a trailing / then do not add one at end */
	const char *separator = (r->uri[strlen(r->uri) - 1] == '/') ? "" : "/";
	fprintf(r->file,"<h1>Index of %s</h1>\r\n",r->uri);
	fprintf(r->file,"<ul>\r\n");
	for (int i = 0; i < n; i++) {
		char *fname = entries[i]->d_name;
		char *mimetype = determine_mimetype(fname);
		bool is_image = strncmp(mimetype, "image/", 6) == 0;
		char webpath[BUFSIZ];

		snprintf(webpath, BUFSIZ, "%s%s%s",r->uri,separator,fname);

		fprintf(r->file,"\t<li>\r\n");
		/* if it's an image add a thumbnail */
		if (is_image) {
			fprintf(r->file,"\t\t<img src=\"%s\" width=\"50\">\r\n",webpath);
		}
		fprintf(r->file,"\t\t<a class=\"btn btn-primary\" href=\"%s\">%s</a>\r\n", webpath, fname);
		fprintf(r->file,"\t</li>\r\n");

		free(mimetype);
		free(entries[i]);
	}
	free(entries);
	fprintf(r->file,"</ul>\r\n");


	/* Flush socket, return OK */
	closedir(d);
	fflush(r->file);
	return HTTP_STATUS_OK;
}

/**
 * Handle file request.
 *
 * @param	r	HTTP Request structure.
 * @param	Status of the HTTP file request.
 *
 * This opens and streams the contents of the specified file to the socket
 *
 * If the path cannot be opened for reading, then handle error with 
 * HTTP_STATUS_NOT_FOUND.
 **/
Status handle_file_request(Request *r) {
	FILE *fs;
	char buffer[BUFSIZ];
	char *mimetype = NULL;
	size_t nread;

	/* Open file for reading */
	fs = fopen(r->path, "r");
	if (!fs) {
		log("Unable to fopen: %s", strerror(errno));
		goto fail;
	}

	/* Determine mimetype */
	mimetype = determine_mimetype(r->path);
	debug("MIME Type: %s", mimetype);

	/* Write HTTP HEADERS with OK status and determined Content-Type */
	fprintf(r->file, "HTTP/1.0 %s\r\n", http_status_string(HTTP_STATUS_OK));
	fprintf(r->file, "Content-type: %s\r\n\r\n", mimetype);

	/* Read from file and write to socket in chunks */
	while ((nread = fread(buffer, 1, BUFSIZ, fs)) > 0) {
		debug("nread = %d", (int)nread);
		if(fwrite(buffer,1,nread, r->file) <= 0) {
			log("fwrite error");
			goto fail;
		}
	}

	/* Close file, flush socket, deallocate mimetype, return OK */
	fclose(fs);
	fflush(r->file);
	free(mimetype);
	return HTTP_STATUS_OK;

fail:
	/* Close file, free mimetype, return INTERNAL_SERVER_ERROR */
	fclose(fs);
	if(mimetype)
		free(mimetype);
	return handle_error(r, HTTP_STATUS_INTERNAL_SERVER_ERROR);
}

/**
 * Handle CGI request
 *
 * @param	r	HTTP Request structure.
 * @return 	Status of the HTTP file request
 *
 * This popens and streams the results of the specified executables to the socket.
 *
 * If the path cannot be popened, then handle error with 
 * HTTP_STATUS_INTERNAL_SERVER_ERROR.
 **/
Status 	handle_cgi_request(Request *r) {
	FILE *pfs;
	char buffer[BUFSIZ];

	/* Export CGI enviornment variables from request 
	 * http://en.wikipedia.org/wiki/Common_Gateway_Interface */
	setenv("DOCUMENT_ROOT",RootPath,1);
	setenv("QUERY_STRING",r->query,1);
	setenv("REMOTE_ADDR",r->host,1);
	setenv("REMOTE_PORT",r->port,1);
	setenv("REQUEST_METHOD",r->method,1);
	setenv("REQUEST_URI",r->uri,1);
	setenv("SCRIPT_FILENAME",r->path,1);
	setenv("SERVER_PORT",Port,1);

	for(struct header *head = r->headers; head ; head = head->next) {
		if(streq(head->name, "Host"))
			setenv("HTTP_HOST",head->value,1);
		else if(streq(head->name,"User-Agent"))
			setenv("HTTP_USER_AGENT",head->value,1);
		else if(streq(head->name,"Accept"))
			setenv("HTTP_ACCEPT",head->value,1);
		else if(streq(head->name,"Accept-Language"))
			setenv("HTTP_ACCEPT",head->value,1);
		else if(streq(head->name,"Accept-Encoding"))
			setenv("HTTP_ACCEPT_ENCODING",head->value,1);
		else if(streq(head->name,"Connection"))
			setenv("HTTP_CONNECTION",head->value,1);
	}

	/* POpen CGI Script */
	pfs = popen(getenv("SCRIPT_FILENAME"),"r");
	if(!pfs) {
		log("failed to POpen: %s", strerror(errno));
		return handle_error(r,HTTP_STATUS_INTERNAL_SERVER_ERROR);
	}	
	
	/* Copy data from popen to socket */
	while (fgets(buffer,BUFSIZ,pfs) && strlen(buffer) > 0) {
		fprintf(r->file, "%s", buffer);
	}

	/* Close popen, flush socket, return OK */
	if (pclose(pfs) == -1) {
		log("failed to pclose: %s", strerror(errno));
	}
	fflush(r->file);
	return HTTP_STATUS_OK;
}

/**
 * Handle displaying error page
 *
 * @param 	r 	HTTP Request structure.
 * @return	Status of the HTTP error request.
 *
 * This writes an HTTP status error code and then generates an HTML message to
 * notify the user of the error.
 **/
Status handle_error(Request *r, Status status) {
	const char *status_string = http_status_string(status);
	FILE *fs;
	char buffer[BUFSIZ];
	size_t nread;

	log("HTTP error: %s", status_string);

	/* Write HTTP Header */
	fprintf(r->file, "HTTP/1.0 %s\n", status_string);
	fprintf(r->file, "Content-type: text/html\r\n\r\n");

	if (status == HTTP_STATUS_NOT_FOUND) {
		/* Open 404 file for reading */
		fs = fopen("www/html/404.html", "r");
		if (!fs) {
			log("Unable to fopen: %s",strerror(errno));
			return status;
		}

		/* Read from file and write to socket in chunks */
		while ((nread = fread(buffer,1,BUFSIZ,fs)) > 0) {
			debug("nread = %d", (int)nread);
			if (fwrite(buffer, 1, nread, r->file) <= 0) {
				log("fwrite error");
			}
		}

		if (fclose(fs) != 0) {
			log("Unable to fclose: %s", strerror(errno));
		}
	} else {
		/* Write HTML Description of Error */
		fprintf(r->file, "<!DOCTYPE html>\n");
		fprintf(r->file, "<html>\n");
		fprintf(r->file, "  <body><h1>%s</h1></body>\n",status_string);
		fprintf(r->file, "</html>\n");
	}

	return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c */
