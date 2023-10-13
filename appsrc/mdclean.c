#define _POSIX_C_SOURCE 200809

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "fcgiapp.h"
#include "mimeparse.h"

#define READ_BUFFER_SIZE 8192
#define READ_BUFFER_BLOCK_SIZE 8192
// 8MiB... this should be the same as nginx configuration
#define READ_BUFFER_MAX_SIZE (8192 * 128 * 8)

#define MDCLEAN_ERROR_NOERROR 0
#define MDCLEAN_ERROR_UNKNOWN -1
#define MDCLEAN_ERROR_HEADERNOTFOUND -2
#define MDCLEAN_ERROR_MEMORYALLOCATIONFAILURE -3
#define MDCLEAN_ERROR_FILECREATIONERROR -4
#define MDCLEAN_ERROR_FILEWRITEFAILURE -5
#define MDCLEAN_ERROR_BOUNDARYNOTFOUND -6
#define MDCLEAN_ERROR_CONTENTTOOLARGE -7
#define MDCLEAN_ERROR_MIMEPARSEFAILURE -8

static const char * mdclean_geterrordescription(int error_code) {
	switch (error_code) {
	case MDCLEAN_ERROR_NOERROR:
		return "No error";
	case MDCLEAN_ERROR_UNKNOWN:
		return "Unknown error";
	case MDCLEAN_ERROR_HEADERNOTFOUND:
		return "Header not found";
	case MDCLEAN_ERROR_MEMORYALLOCATIONFAILURE:
		return "Memory allocation failure";
	case MDCLEAN_ERROR_FILECREATIONERROR:
		return "File creation error";
	case MDCLEAN_ERROR_FILEWRITEFAILURE:
		return "File write failure";
	case MDCLEAN_ERROR_BOUNDARYNOTFOUND:
		return "Boundary not found";
	case MDCLEAN_ERROR_CONTENTTOOLARGE:
		return "Content too large";
	case MDCLEAN_ERROR_MIMEPARSEFAILURE:
		return "MIME parse failure";
	default:
	}
	return "error_code out of range";
}

/*
 * Receive all the data from the input stream
 * save it to a memory buffer
 * and write it to a file
 */
int save_posted_data(FCGX_Request * req) {
	char * read_buffer = NULL;
	size_t read_buffer_size = 0;
	size_t read_buffer_pos = 0;
	char * content_type;
	char * temp_filename = NULL;
	char * boundary = NULL;;
	int res = -1;
	int rc;
	int fd = -1;
	size_t bytes_read, bytes_read_total;
	size_t partlist_idx;
	ssize_t bytes_written;
	mimeparse_part * partlist = NULL;
	size_t partlist_size;
	content_type = FCGX_GetParam("CONTENT_TYPE", req->envp);
	if (content_type == NULL || strlen(content_type) < 6) { res = MDCLEAN_ERROR_HEADERNOTFOUND; goto error; }
	rc = mimeparse_getboundary(&boundary, content_type, strlen(content_type));
	if (rc) { res = MDCLEAN_ERROR_BOUNDARYNOTFOUND; goto error; }
	read_buffer = malloc(READ_BUFFER_SIZE);
	if (!read_buffer) { res = MDCLEAN_ERROR_MEMORYALLOCATIONFAILURE; goto error; }
	read_buffer_size = READ_BUFFER_SIZE;
	temp_filename = strdup("/tmp/mdclean_data.XXXXXX");
	if (!temp_filename) { res = MDCLEAN_ERROR_MEMORYALLOCATIONFAILURE; goto error; }
	fd = mkstemp(temp_filename);
	if (fd == -1) { res = MDCLEAN_ERROR_FILECREATIONERROR; goto error; }
	bytes_read_total = 0;
	do {
		if (read_buffer_pos + READ_BUFFER_BLOCK_SIZE > read_buffer_size) {
			if (read_buffer_pos + READ_BUFFER_BLOCK_SIZE > READ_BUFFER_MAX_SIZE) {
				res = MDCLEAN_ERROR_CONTENTTOOLARGE;
				goto error;
			}
			char * new_read_buffer = realloc(read_buffer, read_buffer_size + READ_BUFFER_BLOCK_SIZE);
			if (new_read_buffer == NULL) {
				res = MDCLEAN_ERROR_MEMORYALLOCATIONFAILURE;
				goto error;
			}
			read_buffer = new_read_buffer;
			read_buffer_size = read_buffer_size + READ_BUFFER_BLOCK_SIZE;
		}
		// Despite the name this function is binary safe.
		bytes_read = FCGX_GetStr(read_buffer + read_buffer_pos, READ_BUFFER_BLOCK_SIZE, req->in);
		// TODO: the whole file is not required anymore
		if (bytes_read) {
			bytes_written = write(fd, read_buffer + read_buffer_pos, bytes_read);
			if (bytes_written != bytes_read) {
				res = MDCLEAN_ERROR_FILEWRITEFAILURE;
				goto error;
			}
			bytes_read_total += bytes_read;
		}
		if (bytes_read) {
			read_buffer_pos = read_buffer_pos + bytes_read;
		}
	} while (bytes_read);
	partlist = mimeparse_parseparts(read_buffer, read_buffer_pos, boundary, &partlist_size);
	if (partlist == NULL) {
		res = MDCLEAN_ERROR_MIMEPARSEFAILURE;
		goto error;
	}
	for (partlist_idx = 0; partlist_idx < partlist_size; partlist_idx++) {
		mimeparse_contentdispositionheader contentdisp =
				mimeparse_parsecontentdisposition(partlist[partlist_idx].header_start,
				partlist[partlist_idx].content_start - partlist[partlist_idx].header_start);
		if (contentdisp.filename) {
			fprintf(stderr, "uploaded filename: %.*s\n", (int)contentdisp.filename_len, contentdisp.filename);
		}
	}
	res = 0;
	FCGX_FPrintF(req->out,
			"Status: 200\r\n"
			"Content-Type: text/html\r\n"
			"\r\n"
			"<!DOCTYPE html>\r\n"
			"<html><head></head><body>\r\n"
			"<h1>POST Data Received</h1>\r\n"
			"<p>Received %d bytes of POST data, written to %s</p>\r\n"
			"<p><a href=\"posttest\">Return to form page</a></p>\r\n"
			"</body></html>\r\n",
			(int)bytes_read_total,
			temp_filename);
	// demonstrate how redirections are configured :-
	//   0: stdin is actually connected to the unix domain socket (it means we can read and
	//      write to that file descriptor)
	//   1: stdout - as usual
	//   2: stderr - as usual
	fprintf(stderr, "stderr: %d bytes saved to %s\n", (int)bytes_read_total, temp_filename);
	fprintf(stdout, "stdout: %d bytes saved to %s\n", (int)bytes_read_total, temp_filename);
	goto finish;
error:
	FCGX_FPrintF(req->out,
			"Status: 500\r\n"
			"Content-Type: text/html\r\n"
			"\r\n"
			"<!DOCTYPE html>\r\n"
			"<html><head></head><body>\r\n"
			"<h1>FastCGI Server Error</h1>\r\n"
			"<p>%s</p>\r\n"
			"</body></html>\r\n",
			mdclean_geterrordescription(res));
	fprintf(stderr, "stderr: save_posted_data() %d: %s\n", res, mdclean_geterrordescription(res));
finish:
	FCGX_FFlush(req->out);
	if (partlist) {
		free(partlist);
	}
	if (fd != -1) {
		close(fd);
	}
	if (temp_filename) {
		free(temp_filename);
	}
	if (read_buffer) {
		free(read_buffer);
	}
	if (boundary) {
		free(boundary);
	}
	return res;
}

/*
 * Determine the users request and act appropriately
 * Support for:
 *  GET test
 *  GET posttest
 *  POST posttest
 *  GET <anything else>
 *  POST <anything else>
 *  Other Verbs return a 405
 * These paths are taken from PATH_INFO and so are relative.
 * e.g. a GET request to /mdclean/test would set PATH_INFO to test.
 */
int process_request(FCGX_Request * request) {
	char * request_method = FCGX_GetParam("REQUEST_METHOD", request->envp);
	// https://nginx.org/en/docs/http/ngx_http_fastcgi_module.html#fastcgi_split_path_info
	char * path_info = FCGX_GetParam("PATH_INFO", request->envp);
	if (path_info == NULL) {
		FCGX_FPrintF(request->out,
				"Status: 500\r\n"
				"Content-Type: text/html\r\n"
				"\r\n"
				"<!DOCTYPE html>\r\n"
				"<html><head><meta charset=\"utf-8\"></head><body>\r\n"
				"<h1>FastCGI Server Error</h1>\r\n"
				"<p><code>PATH_INFO</code> variable not found.</p>\r\n"
				"</body></html>");
		goto finish;
	}
	if (strcmp(request_method, "GET") == 0) {
		if (strcmp(path_info, "test") == 0) {
			FCGX_FPrintF(request->out,
					"Status: 200\r\n"
					"Content-Type: text/html\r\n"
					"\r\n"
					"<!DOCTYPE html>\r\n"
					"<html><head><meta charset=\"utf-8\"></head><body>\r\n"
					"<h1>mdclean from fcgi application</h1>\r\n"
					"<p>Path matched to <code>test</code>\r\n"
					"</body></html>\r\n");
		} else
		if (strcmp(path_info, "posttest") == 0) {
			FCGX_FPrintF(request->out,
					"Status: 200\r\n"
					"Content-Type: text/html\r\n"
					"\r\n"
					"<!DOCTYPE html>\r\n"
					"<html><head><meta charset=\"utf-8\"></head><body>\r\n"
					"<h1>POST some data</h1>\r\n"
					"<p>Data posted with this form will be saved raw in this server's /tmp directory</p>\r\n"
					"<form method=\"POST\" enctype=\"multipart/form-data\">\r\n"
					"<label for=\"tags\">Tags:</label><input type=\"text\" name=\"tags\"><br/>\r\n"
					"<label for=\"file\">File:</label><input type=\"file\" name=\"file\"><br/>\r\n"
					"<input type=\"submit\" value=\"Upload\"/>\r\n"
					"</form>\r\n"
					"</body></html>\r\n");
		} else {
			goto send_404;
		}
	} else
	if (strcmp(request_method, "POST") == 0) {
		char * path_info = FCGX_GetParam("PATH_INFO", request->envp);
		if (path_info != NULL && strcmp(path_info, "posttest") == 0) {
			save_posted_data(request);
		} else {
send_404:
			FCGX_FPrintF(request->out,
					"Status: 404\r\n"
					"Content-Type: text/html\r\n"
					"\r\n"
					"<!DOCTYPE html>\r\n"
					"<html><head><meta charset=\"utf-8\"></head><body>\r\n"
					"<h1>File Not Found</h1>\r\n"
					"<p>For a 200 response try sending a GET to test</p>\r\n"
					"</body></html>\r\n");
		}
	} else {
		/* HTTP Status 405 Method Not Allowed for unsupported verbs */
		FCGX_FPrintF(request->out,
				"Status: 405\r\n"
				"Content-Type: text/html\r\n"
				"\r\n"
				"<!DOCTYPE html>\r\n"
				"<html><head><meta charset=\"utf-8\"></head><body>\r\n"
				"<h1>Method Not Allowed</h1>\r\n"
				"<p>Requested Method: <code>%s</code></p>\r\n"
				"</body></html>\r\n",
				request_method);
	}
finish:
	FCGX_FFlush(request->out);
	FCGX_Finish_r(request);
	return 0;
}

int main(int argc, char *argv[]) {
	FCGX_Request * request;
	int rc;
	
	FCGX_Init();
	request = (FCGX_Request *)malloc(sizeof(FCGX_Request));
	if (request == NULL) { return 1; }
	rc = FCGX_InitRequest(request, 0, 0);
	if (rc != 0) {
		fprintf(stdout, "Error InitRequest(): %d\n", rc);
		goto error;
	}
	for (;;) {
		rc = FCGX_Accept_r(request);
		if (rc == 0) {
			rc = process_request(request);
		} else {
			fprintf(stdout, "Error Accept_r(): %d\n", rc);
			goto error;
		}
	}
error:
	free(request);
	return 0;
}


