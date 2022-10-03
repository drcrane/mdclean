#define _POSIX_C_SOURCE 200809

#include <string.h>
#include <unistd.h> // close, write, mkstemp
#include <malloc.h>
#include <stddef.h>
#include <fcgiapp.h>
#include "dbg.h"
#include "formdecoder.h"
#include "dynstring.h"
#include "contentdisposition.h"
#include "errorinfo.h"
#include "querystring.h"
#include "simplemap.h"

struct formdecoder_context {
	int type;

	simplemap_context * fields;
	struct formdecoder_context_field * current_field;

	dynstring_context_t current_field_data;
	char * buf;
	querystring_context * qsctx;
	int save_files;
};

static char * formdecoder_strdup(char * str) {
	size_t len = strlen(str);
	len ++;
	char * new_str = malloc(len);
	memcpy(new_str, str, len);
	return new_str;
}

formdecoder_context * formdecoder_init() {
	formdecoder_context * ctx;
	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) { return NULL; }
	dynstring_initialisezero(&ctx->current_field_data);
	ctx->fields = simplemap_create(10);
	ctx->buf = NULL;
	ctx->type = FORMDECODER_MULTIPART;
	ctx->qsctx = NULL;
	ctx->save_files = 0;
	return ctx;
}

/*
 * Values returned are null terminated by implementation.
 * Be careful if altered!
 */
int formdecoder_takefieldvalue(formdecoder_context * ctx, char * key, size_t * data_len, char ** data) {
	int res = -1;
	size_t key_len;
	struct formdecoder_context_field * removed;
	size_t removed_sz;
	key_len = strlen(key);
	res = simplemap_removebykey(ctx->fields, key, key_len, 0, (void **)&removed, &removed_sz);
	if (res == 0) {
		*data_len = removed->data_len;
		*data = removed->data;
		free(removed);
	}
	return res;
}

static void formdecoder_dispose_formfield(struct formdecoder_context_field * field) {
	if (field->data) {
		free(field->data);
	}
	if (field->mime_type) {
		free(field->mime_type);
	}
	if (field->filename) {
		free(field->filename);
	}
	if (field->fd != -1) {
		close(field->fd);
		field->fd = -1;
	}
	if (field->tempfilename) {
		debug("disposing tempfilename %s", field->tempfilename);
		unlink(field->tempfilename);
		free(field->tempfilename);
	}
	free(field);
}

void formdecoder_dispose(formdecoder_context * ctx) {
	if (ctx->buf) {
		free(ctx->buf);
	}
	if (ctx->qsctx) {
		querystring_dispose(ctx->qsctx);
	}
	dynstring_free(&ctx->current_field_data);
	simplemap_dispose_cb(ctx->fields, (simplemap_datacallback)formdecoder_dispose_formfield);
	free(ctx);
}

int formdecoder_setfield(formdecoder_context * ctx, char * key, size_t data_len, char * data) {
	size_t key_len;
	size_t field_len;
	struct formdecoder_context_field * replaced;
	struct formdecoder_context_field * field;
	struct formdecoder_context_field * to_insert;
	int res;
	key_len = strlen(key);
	res = simplemap_findfromkey(ctx->fields, key, key_len, 0, (void **)&field, &field_len);
	if (res == 0) {
		return -1;
	}
	to_insert = malloc(sizeof(struct formdecoder_context_field));
	if (to_insert == NULL) { return -1; }
	memset(to_insert, 0, sizeof(struct formdecoder_context_field));
	to_insert->data = data;
	to_insert->data_len = data_len;
	replaced = NULL;
	res = simplemap_addorreplace(ctx->fields, key, key_len, 0, (void **)&replaced, to_insert, sizeof(struct formdecoder_context_field));
	if (replaced) {
		formdecoder_dispose_formfield(replaced);
	}
	return res;
}

int formdecoder_getfield(formdecoder_context * ctx, char * key, size_t * data_len_ptr, char ** data_ptr) {
	if (ctx->type == FORMDECODER_QUERYSTRING) {
		char * field_value;
		field_value = querystring_getbykey(ctx->qsctx, key);
		if (field_value) {
			if (data_len_ptr != NULL) {
				*data_len_ptr = strlen(field_value);
			}
			*data_ptr = field_value;
			return 0;
		}
	} else
	if (ctx->type == FORMDECODER_MULTIPART) {
		struct formdecoder_context_field * field;
		int res;
		size_t key_len;
		size_t field_len;
		key_len = strlen(key);
		res = simplemap_findfromkey(ctx->fields, key, key_len, 0, (void **)&field, &field_len);
		*data_ptr = field->data;
		*data_len_ptr = field->data_len;
		return res;
	}
	return -1;
}

size_t formdecoder_fieldcount(formdecoder_context * ctx) {
	return simplemap_count(ctx->fields);
}

int formdecoder_mimedecoder_callback(mimedecoder_context * ctx, int event, void * ptr, size_t ptr_size) {
	formdecoder_context * fd_ctx;
	fd_ctx = (formdecoder_context *)ctx->user;
	switch (event) {
	case MIMEDECODER_EVENT_HEADER:
		//debug("formdecoder MIMEDECODER_EVENT_HEADER");
		if (strncmp(ctx->header_name.buf, "Content-Disposition", ctx->header_name.pos) == 0) {
			if (strncmp(ctx->header_value.buf, "form-data", 9) == 0) {
				char * name = NULL;
				char * filename = NULL;
				name = contentdisposition_getname(ctx->header_value.buf);
				if (name) {
					fd_ctx->current_field = malloc(sizeof(struct formdecoder_context_field));
					if (fd_ctx->current_field == NULL) {
						debug("Current Field is NULL");
						return -1;
					}
					memset(fd_ctx->current_field, 0, sizeof(struct formdecoder_context_field));
					fd_ctx->current_field->fd = -1;
					simplemap_append(fd_ctx->fields, name, strlen(name), fd_ctx->current_field, sizeof(struct formdecoder_context_field));
					free(name);
				}
				filename = contentdisposition_getfilename(ctx->header_value.buf);
				if (fd_ctx->current_field && filename && fd_ctx->save_files) {
					int fd;
					char * tempfilename = formdecoder_strdup("/tmp/formdecoder.XXXXXX");
					debug("formdecoder FILE DETECTED: %s", filename);
					//free(filename);
					fd_ctx->current_field->filename = filename;
					fd = mkstemp(tempfilename);
					if (fd == -1) {
						debug("Error creating temp file!");
						free(tempfilename);
						return -1;
					}
					fd_ctx->current_field->tempfilename = tempfilename;
					fd_ctx->current_field->fd = fd;
					debug("formdecoder CREATE FILE %s", tempfilename);
				}
			}
		} else
		if (strncmp(ctx->header_name.buf, "Content-Type", ctx->header_name.pos) == 0) {
			// Relies on the header_value being nul terminated
			fd_ctx->current_field->mime_type = formdecoder_strdup(ctx->header_value.buf);
			debug("Content-Type: %s", fd_ctx->current_field->mime_type);
		}
		break;
	case MIMEDECODER_EVENT_NEW_PART:
		break;
	case MIMEDECODER_EVENT_BODY_START:
		if (fd_ctx->current_field_data.buf != NULL) {
			debug("current_field_data is not NULL");
			return -1;
		}
		if (fd_ctx->current_field->fd == -1) {
			dynstring_initialise(&fd_ctx->current_field_data, 64);
		}
		break;
	case MIMEDECODER_EVENT_BODY_CONTENT:
	{
		struct formdecoder_context_field * current_field;
		ssize_t rc;
		current_field = fd_ctx->current_field;
		current_field->data_len += ptr_size;
		if (current_field->fd == -1) {
			dynstring_appendstring(&fd_ctx->current_field_data, (char *)ptr, ptr_size);
		} else {
			rc = write(current_field->fd, ptr, ptr_size);
			if (rc != ptr_size) {
				debug("rc != ptr_size in EVENT_BODY_CONTENT (fd %d)", current_field->fd);
				return -1;
			}
		}
		//debug("MIMEDECODER_EVENT_BODY_CONTENT appended %d bytes", (int)ptr_size);
	}
		break;
	case MIMEDECODER_EVENT_BODY_END:
		if (fd_ctx->current_field->fd == -1) {
			fd_ctx->current_field->data_len = dynstring_length(&fd_ctx->current_field_data);
			// detaching the string moves responsibility for this memory away from the dynstring
			fd_ctx->current_field->data = dynstring_detachcstring(&fd_ctx->current_field_data);
		} else {
			close(fd_ctx->current_field->fd);
			fd_ctx->current_field->fd = -1;
		}
		break;
	default:
		debug("executed default for some reason");
		return -1;
	}
	return 0;
}

#define READ_BUFFER_SIZE 8192

//#define FORMDECODER_WRITEPOSTEDDATA

static int formdecoder_decodefcgirequest_multipart(FCGX_Request * req, formdecoder_context ** fd_ctx_ptr, size_t max_size, errorinfo_context ** ei_ctx_ptr) {
	mimedecoder_context * mi_ctx = NULL;
	formdecoder_context * fd_ctx = NULL;
	char * content_type;
	char * read_buffer;
	size_t bytes_read, bytes_read_total;
	int rc;
#ifdef FORMDECODER_WRITEPOSTEDDATA
	FILE * file;
	file = fopen("/tmp/formdecoder-postdata.tmp", "w");
	if (file == NULL) {
		debug("could not create full post output file (/tmp/formdecoder-postdata.tmp)");
		return -1;
	}
#endif /* FORMDECODER_WRITEPOSTEDDATA */
	fd_ctx = formdecoder_init();
	if (fd_ctx == NULL) {
		goto error_none;
	}
	fd_ctx->save_files = 1;
	mi_ctx = mimedecoder_init();
	if (mi_ctx == NULL) {
		goto error_fd;
	}
	mi_ctx->max_size = max_size;
	content_type = FCGX_GetParam("CONTENT_TYPE", req->envp);
	rc = mimedecoder_setcontenttype(mi_ctx, content_type);
	if (rc) {
		errorinfo_createbasic(ei_ctx_ptr, "mimedecoder_setcontenttype() %d", rc);
		goto error_mi;
	}
	mi_ctx->user = fd_ctx;
	mi_ctx->cb = &formdecoder_mimedecoder_callback;
	read_buffer = malloc(READ_BUFFER_SIZE);
	if (read_buffer == NULL) {
		goto error_mi;
	}
	bytes_read_total = 0;
	do {
		bytes_read = FCGX_GetStr(read_buffer, READ_BUFFER_SIZE, req->in);
		if (bytes_read) {
#ifdef FORMDECODER_WRITEPOSTEDDATA
			rc = fwrite(read_buffer, 1, bytes_read, file);
			debug("fwrite() %d", rc);
#endif /* FORMDECODER_WRITEPOSTEDDATA */
			rc = mimedecoder_decode(mi_ctx, read_buffer, bytes_read);
			if (rc) {
				if (mi_ctx->substate == MIMEDECODER_SUBSTATE_ERROR_CONTENTTOOLARGE) {
					errorinfo_createbasic(ei_ctx_ptr, "MIMEDECODER_SUBSTATE_ERROR_CONTENTTOOLARGE rc = %d", rc);
				} else
				if (mi_ctx->substate == MIMEDECODER_SUBSTATE_ERROR_UNKNOWN) {
					errorinfo_createbasic(ei_ctx_ptr, "MIMEDECODER_SUBSTATE_ERROR_UNKNOWN rc = %d", rc);
				} else {
					errorinfo_createbasic(ei_ctx_ptr, "MIMEDECODER_SUBSTATE_ERROR_%d rc = %d", mi_ctx->substate, rc);
				}
				goto error_rb;
			}
			bytes_read_total += bytes_read;
		}
	} while (bytes_read > 0);
#ifdef FORMDECODER_WRITEPOSTEDDATA
	fclose(file);
#endif /* FORMDECODER_WRITEPOSTEDDATA */
	free(read_buffer);
	read_buffer = NULL;
	rc = mimedecoder_finalise(mi_ctx);
	mi_ctx = NULL;
	if (rc) {
		goto error_fd;
	}
	*fd_ctx_ptr = fd_ctx;
	return 0;
error_rb:
	free(read_buffer);
error_mi:
	mimedecoder_dispose(mi_ctx);
error_fd:
	formdecoder_dispose(fd_ctx);
error_none:
#ifdef FORMDECODER_WRITEPOSTEDDATA
	fclose(file);
#endif /* FORMDECODER_WRITEPOSTEDDATA */
	return -1;
}

static int formdecoder_decodefcgirequest_urlencoded(FCGX_Request * req, formdecoder_context ** fd_ctx_ptr, size_t max_size, errorinfo_context ** ei_ctx_ptr) {
	formdecoder_context * fd_ctx = NULL;
	char * read_buf;
	size_t bytes_read, bytes_read_total;
	size_t bytes_to_read;
	fd_ctx = formdecoder_init();
	if (fd_ctx == NULL) {
		return -1;
	}
	fd_ctx->type = FORMDECODER_QUERYSTRING;
	read_buf = malloc(max_size);
	if (read_buf == NULL) {
		goto error_fd;
	}
	bytes_read_total = 0;
	/* read all the bytes into the buffer */
	do {
		bytes_to_read = max_size - bytes_read_total;
		if (bytes_to_read == 0) {
			errorinfo_createbasic(ei_ctx_ptr, "read buffer full");
			goto error_rb;
		}
		bytes_read = FCGX_GetStr(read_buf + bytes_read_total, bytes_to_read, req->in);
		if (bytes_read) {
			/* do some decoding of the read buffer */
			bytes_read_total += bytes_read;
		}
	} while (bytes_read > 0);
	if (bytes_read_total >= max_size) {
		goto error_rb;
	}
	read_buf[bytes_read_total] = '\0';
	fd_ctx->qsctx = querystring_decode_inplace(read_buf);
	if (fd_ctx->qsctx == NULL) {
		errorinfo_createbasic(ei_ctx_ptr, "Failed to decode querystring");
		goto error_rb;
	}
	fd_ctx->buf = read_buf;
	*fd_ctx_ptr = fd_ctx;
	return 0;
error_rb:
	free(read_buf);
error_fd:
	formdecoder_dispose(fd_ctx);
	return -1;
}

/*
 * Take a FCGX_Request (check that it is a POST) and decode the stream.
 * for ease of implementation this will handle both multipart mime style
 * post data and urlencoded data.
 * formdecoder_context will contain the result of the decoding if successful.
 * max_size should set the maximum size in bytes that will be accepted.
 * errorinfo_context should contain some information on what went wrong.
 * Return: 0 on success
 * Return: -1 on failure
 */
int formdecoder_decodefcgirequest(FCGX_Request * req, formdecoder_context ** fd_ctx_ptr, size_t max_size, errorinfo_context ** ei_ctx_ptr) {
	char * request_method;
	char * content_type;
	request_method = FCGX_GetParam("REQUEST_METHOD", req->envp);
	if (request_method == NULL || strcmp("POST", request_method) != 0) {
		errorinfo_createbasic(ei_ctx_ptr, "Request method was not POST");
		return -1;
	}
	content_type = FCGX_GetParam("CONTENT_TYPE", req->envp);
	if (content_type == NULL) {
		errorinfo_createbasic(ei_ctx_ptr, "No content-type header provided");
		return -1;
	}
	if (strncmp("multipart/form-data", content_type, 19) == 0) {
		return formdecoder_decodefcgirequest_multipart(req, fd_ctx_ptr, max_size, ei_ctx_ptr);
	} else
	if (strncmp("application/x-www-form-urlencoded", content_type, 33) == 0) {
		return formdecoder_decodefcgirequest_urlencoded(req, fd_ctx_ptr, max_size, ei_ctx_ptr);
	} else {
		errorinfo_createbasic(ei_ctx_ptr, "Unrecognised content type \"%s\"", content_type);
		fprintf(stderr, "Unrecognised content type \"%s\"\n", content_type);
		return -1;
	}
	return -1;
}

