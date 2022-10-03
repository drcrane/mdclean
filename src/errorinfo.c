#include <string.h>
#include "dynstring.h"
#include "dynstringprintf.h"
#include "linkedlist.h"
#include "errorinfo.h"

struct errorinfoobject_context {
	int type;
	void * data;
	void (*free_cb)(void *);
};

struct errorinfo_context {
	dynstring_context_t message;
	size_t infolist_capacity;
	size_t infolist_count;
	struct errorinfoobject_context infolist[10];
	struct errorinfo_context * cause;
};

errorinfo_context * errorinfo_create() {
	errorinfo_context * ei_ctx;
	int rc;
	ei_ctx = malloc(sizeof(*ei_ctx));
	if (ei_ctx == NULL) {
		return NULL;
	}
	ei_ctx->infolist_capacity = 10;
	ei_ctx->infolist_count = 0;
	ei_ctx->cause = NULL;
	rc = dynstring_initialise(&ei_ctx->message, 1024);
	if (rc) {
		free(ei_ctx);
		return NULL;
	}
	return ei_ctx;
}

int errorinfo_createbasic(errorinfo_context ** ei_ctx_ptr, char * fmt, ...) {
	errorinfo_context * ei_ctx;
	va_list args;
	va_start(args, fmt);
	if (ei_ctx_ptr == NULL || *ei_ctx_ptr != NULL) {
		return -1;
	}
	ei_ctx = errorinfo_wrap_va(NULL, fmt, args);
	va_end(args);
	if (ei_ctx == NULL) {
		return -1;
	}
	*ei_ctx_ptr = ei_ctx;
	return 0;
}

void errorinfo_dispose(errorinfo_context * ei_ctx) {
	size_t i;
	if (ei_ctx == NULL) {
		return;
	}
	if (ei_ctx->cause) {
		errorinfo_dispose(ei_ctx->cause);
		ei_ctx->cause = NULL;
	}
	for (i = 0; i < ei_ctx->infolist_count; i++) {
		if (ei_ctx->infolist[i].free_cb != NULL) {
			ei_ctx->infolist[i].free_cb(ei_ctx->infolist[i].data);
		}
	}
	dynstring_free(&ei_ctx->message);
	free(ei_ctx);
}

int errorinfo_appendmessage(errorinfo_context * ei_ctx, char * message) {
	int rc;
	rc = dynstring_appendstringz(&ei_ctx->message, message, "\n", NULL);
	return rc;
}

int errorinfo_addinfoobject(errorinfo_context * ei_ctx, int type, void * data, void (*free_cb)(void *)) {
	if (ei_ctx->infolist_count < ei_ctx->infolist_capacity) {
		struct errorinfoobject_context * eio_ctx;
		eio_ctx = &ei_ctx->infolist[ei_ctx->infolist_count];
		eio_ctx->type = type;
		eio_ctx->data = data;
		eio_ctx->free_cb = free_cb;
		ei_ctx->infolist_count ++;
		return 0;
	}
	return -1;
}

errorinfo_context * errorinfo_wrap_va(errorinfo_context * cause_ei_ctx, char * fmt, va_list args) {
	int rc;
	errorinfo_context * ei_ctx;
	ei_ctx = errorinfo_create();
	if (!ei_ctx) {
		goto end;
	}
	rc = dynstring_sprintf_va(&ei_ctx->message, fmt, args);
	if (!rc) {
		errorinfo_dispose(ei_ctx);
		ei_ctx = NULL;
		goto end;
	}
	ei_ctx->cause = cause_ei_ctx;
end:
	return ei_ctx;
}

errorinfo_context * errorinfo_wrap(errorinfo_context * cause_ei_ctx, char * fmt, ...) {
	errorinfo_context * ei_ctx;
	va_list args;
	va_start(args, fmt);
	ei_ctx = errorinfo_wrap_va(cause_ei_ctx, fmt, args);
	va_end(args);
	return ei_ctx;
}

char * errorinfo_cstr(errorinfo_context * ei_ctx) {
	if (ei_ctx == NULL) {
		return "errorinfo_context was null";
	}
	return dynstring_getcstring(&ei_ctx->message);
}

