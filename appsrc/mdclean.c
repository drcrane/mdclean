#define _POSIX_C_SOURCE 200809

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "dynstring.h"
#include "mimedecoder.h"
#include "formdecoder.h"
#include "errorinfo.h"
#include "strutils.h"
#include "fcgiapp.h"
#include "extprocess.h"

#define READ_BUFFER_SIZE 8192

static int write_file_to_fastcgi(char * filename, FCGX_Stream * out) {
	char * buf;
	int fd;
	int rc;
	int res = 0;
	ssize_t br;
	int ctr;
	buf = malloc(READ_BUFFER_SIZE);
	if (buf == NULL) {
		return -1;
	}
	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		res = -1;
		goto error;
	}
	ctr = 1024;
	do {
		br = read(fd, buf, READ_BUFFER_SIZE);
		if (br <= 0) {
			break;
		}
		rc = FCGX_PutStr(buf, br, out);
		if (rc < 0 || rc != br) {
			res = -1;
			break;
		}
	} while (--ctr);
	close(fd);
error:
	if (buf) {
		free(buf);
	}
	return res;
}

int process_posted_data(FCGX_Request * req) {
	char * error_str = "NO ERROR DETECTED";
	char * content_type;
	char * file_content_type;
	formdecoder_context * fd_ctx;
	errorinfo_context * ei_ctx = NULL;
	struct formdecoder_context_field * form_field;
	char * filename;
	int rc;
	content_type = FCGX_GetParam("CONTENT_TYPE", req->envp);
	if (content_type) {
		fprintf(stdout, "Content-Type: %s\n", content_type);
	}
	if (content_type == NULL || !strutils_begins_with(content_type, "multipart/form-data")) {
		error_str = "invalid or unsupplied Content-Type header";
		goto error;
	}
	rc = formdecoder_decodefcgirequest(req, &fd_ctx, 8LL * 1024LL * 1024LL, &ei_ctx);
	if (rc == -1) {
		error_str = errorinfo_cstr(ei_ctx);
		goto error;
	}
	form_field = formdecoder_getfieldex(fd_ctx, "file");
	if (form_field == NULL) {
		error_str = "Field not found in request";
		goto error;
	}
	filename = form_field->filename;
	if (filename == NULL) {
		error_str = "filename not found in form field";
		goto error;
	}
	file_content_type = form_field->mime_type;
	if (file_content_type == NULL) {
		error_str = "mime type not found in form field";
		goto error;
	}
	FCGX_FPrintF(req->out,
			"Status: 200\r\n"
			"Content-Type: %s\r\n"
			"Content-Disposition: attachment; filename=%s;\r\n"
			"\r\n",
			file_content_type,
			filename);
	rc = write_file_to_fastcgi(form_field->tempfilename, req->out);
	if (rc) {
		fprintf(stderr, "write_file_to_fastcgi() rc = %d\r\n", rc);
	}
	goto finish;
error:
	FCGX_FPrintF(req->out,
			"Status: 400\r\n"
			"Content-Type: text/html; charset=utf-8\r\n"
			"\r\n"
			"<!DOCTYPE html>\r\n"
			"<html><head></head><body>\r\n"
			"<h1>400 Client Error</h1>\r\n"
			"<p>Error Processing Request</p>\r\n"
			"<code>%s</code>\r\n"
			"</body></html>\r\n",
			error_str);
	fprintf(stderr, "%s\n", error_str);
finish:
	if (fd_ctx) {
		formdecoder_dispose(fd_ctx);
	}
	if (ei_ctx) {
		errorinfo_dispose(ei_ctx);
	}
	return 0;
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
					"<p>Data posted with this form will be returned to you unchanged with <code>Content-Disposition: attachment</code></p>\r\n"
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
			process_posted_data(request);
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


