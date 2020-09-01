#define _POSIX_C_SOURCE 200809

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fcgiapp.h"

#define READ_BUFFER_SIZE 8192

/*
 * Receive all the data from the input stream and write it to a file
 */
int save_posted_data(FCGX_Request * req) {
	char * read_buffer = NULL;
	char * content_type;
	char * temp_filename = NULL;
	int res = -1;
	int fd = -1;
	size_t bytes_read, bytes_read_total;
	ssize_t bytes_written;
	content_type = FCGX_GetParam("CONTENT_TYPE", req->envp);
	if (content_type == NULL) { goto error; }
	read_buffer = malloc(READ_BUFFER_SIZE);
	if (!read_buffer) { goto error; }
	temp_filename = strdup("/tmp/mdclean_data.XXXXXX");
	if (!temp_filename) { goto error; }
	fd = mkstemp(temp_filename);
	if (fd == -1) { goto error; }
	bytes_read_total = 0;
	do {
		// Despite the name this function is binary safe.
		bytes_read = FCGX_GetStr(read_buffer, READ_BUFFER_SIZE, req->in);
		if (bytes_read) {
			bytes_written = write(fd, read_buffer, bytes_read);
			if (bytes_written != bytes_read) {
				goto error;
			}
			bytes_read_total += bytes_read;
		}
	} while (bytes_read);
	res = 0;
	FCGX_FPrintF(req->out,
			"Status: 200\r\n"
			"Content-Type: text/html\r\n"
			"\r\n"
			"<!DOCTYPE html>\r\n"
			"<html><head></head><body>\r\n"
			"<h1>POST Data Received</h1>\r\n"
			"<p>Received %d bytes of POST data, written to %s</p>"
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
			"</body></html>\r\n");
finish:
	FCGX_FFlush(req->out);
	if (fd != -1) {
		close(fd);
	}
	if (temp_filename) {
		free(temp_filename);
	}
	if (read_buffer) {
		free(read_buffer);
	}
	return res;
}

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
			goto send_404;
		}
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


