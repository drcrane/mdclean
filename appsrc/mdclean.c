
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fcgiapp.h"

int process_request(FCGX_Request * request) {
	char * request_method = FCGX_GetParam("REQUEST_METHOD", request->envp);
	if (strcmp(request_method, "GET") == 0) {
		char * response;
		// https://nginx.org/en/docs/http/ngx_http_fastcgi_module.html#fastcgi_split_path_info
		char * path_info = FCGX_GetParam("PATH_INFO", request->envp);
		if (path_info != NULL && strcmp(path_info, "test") == 0) {
			FCGX_FPrintF(request->out,
					"Status: 200\r\n"
					"Content-Type: text-html\r\n"
					"\r\n"
					"<!DOCTYPE html>\r\n"
					"<html><head></head><body>\r\n"
					"<h1>mdclean from fcgi application</h1>\r\n"
					"<p>Path matched to <code>test</code>\r\n"
					"</body></html>\r\n");
		} else {
			FCGX_FPrintF(request->out,
					"Status: 404\r\n"
					"Content-Type: text/html\r\n"
					"\r\n"
					);
			response = "<h1>File Not Found</h1>\r\n"
					"<p>For a 200 response try test</p>\r\n";
			FCGX_PutStr(response, strlen(response), request->out);
		}
		FCGX_FFlush(request->out);
	}
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


