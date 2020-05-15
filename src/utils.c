/* utils.c: main utilities */

#include "main.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Determine mime-type from file extension.
 *
 * @param	path	Path to file.
 * @return 	An allocated string containing the mime-type of the specified file.
 *
 * This function first finds the file's extension and then scans the contents
 * of the MimeTypesPath file to determine which mimetype the file has.
 *
 * The MimeTypesPath file (typically /etc/mime.types) consists of rules in the 
 * following format:
 *
 * <MIMETYPE>	<EXT1> <EXT2> ...
 *
 * This function simply checks the file extension version each extension for 
 * each mimetype and returns the mimetype on first match.
 *
 * If no extension exists or no mathing mimetype is found, then return
 * DefaultMimeType.
 *
 * This function returns an allocated string that must be freed
 **/
char * determine_mimetype(const char *path) {
	char *ext;
	char *mimetype;
	char *token;
	char buffer[BUFSIZ];
	FILE *fs = NULL;

	/* Find file extension */
	ext = strrchr(path, '.');
	if(!ext || ext == path) {
		debug("Unable to find extension");
		return strdup(DefaultMimeType);
	}
	debug("Extension: %s", ext);

	/* Open MimeTypesPath file */
	fs = fopen(MimeTypesPath, "r");
	if (!fs) {
		log("Unable to fopen MimeTypesPath: %s", strerror(errno));
		return strdup(DefaultMimeType);
	}

	ext++;	// skip over the .

	/* Scan file for matching file extensions */
	while (fgets(buffer, BUFSIZ, fs)) {
		mimetype = strtok(buffer, WHITESPACE);

		while((token = strtok(NULL,WHITESPACE))) {
			if (streq(token,ext)) {
				fclose(fs);
				return strdup(mimetype);
			}
		}
	}

	/* If we've reached this without returning, MIME wasn't found */
	debug("Mime type not found for %s",ext);
	fclose(fs);
	return strdup(DefaultMimeType);
}

/**
 * Determine actual filesystem path based on RootPath and URI.
 *
 * @param	uri	Resource path of URI.
 * @return 	An allocated string containing the full path of the resource on the
 * local filesystem
 *
 * This function uses realpath(3) to generate the realpath of the file request in
 * the URI
 *
 * As a security check, if the real path does not begin with RootPath, then
 * return NULL
 *
 * Otherwise, return a newly allocated string containing the real path. This string
 * must later be freed
 **/
char * determine_request_path(const char *uri) {
	char fulluri[BUFSIZ];
	snprintf(fulluri,BUFSIZ, "%s%s",RootPath,uri);
	debug("URI: %s", fulluri);
	
	char rp[PATH_MAX+1];
	char *result = realpath(fulluri,rp);
	if(!result) {
		perror("realpath");
		return NULL;
	}
	debug("realpath result: %s", rp);
	debug("RootPath = %s", RootPath);
	if(result && strncmp(rp, RootPath, strlen(RootPath)) == 0) {
		return strdup(rp);
	} else {
		return NULL;
	}
}


/**
 * Return static string corresponding to HTTP Status code.
 *
 * @param 	status		HTTP Status.
 * @return	Corresponding HTTP Status string (or NULL if not present).
 **/

const char * http_status_string(Status status) {
	static char *StatusStrings[] = {
		"200 OK",
		"400 Bad Request",
		"404 Not Found",
		"500 Internal Server Error",
		"418 I'm A Teapot"
	};

	if (status == HTTP_STATUS_OK) {
		return StatusStrings[0];
	}
	else if (status == HTTP_STATUS_BAD_REQUEST) {
		return StatusStrings[1];
	}
	else if (status == HTTP_STATUS_NOT_FOUND) {
		return StatusStrings[2];
	}
	else if (status == HTTP_STATUS_INTERNAL_SERVER_ERROR) {
		return StatusStrings[3];
	}
	else {
		return StatusStrings[4];
	}
}

/**
 * Advance string pointer pass all nonwhitespace characters
 *
 * @param 	s	String.
 * @return 	Point to first whitespace character in s.
 **/
char * skip_nonwhitespace(char *s) {
	while(!isspace(*s)){
		s++;
	}
	return s;
}

/**
 * Advance string pointer pass all whitespace characters
 *
 * @param	s	String.
 * @return	Point ot first non-witespace character in s.
 **/
char * skip_whitespace(char *s) {
	while (isspace(*s)){
		s++;
	}
	return s;
}


/* vim: set expandtab sts=4 sw=4 ts=8 ft=c */
