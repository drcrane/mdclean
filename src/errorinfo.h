#ifndef ERRORINFO_H
#define ERRORINFO_H

#include <stdarg.h>

struct errorinfo_context;
typedef struct errorinfo_context errorinfo_context;

errorinfo_context * errorinfo_create();
int errorinfo_createbasic(errorinfo_context ** ei_ctx_ptr, char * fmt, ...);
void errorinfo_dispose(errorinfo_context * ei_ctx);

errorinfo_context * errorinfo_wrap_va(errorinfo_context * cause_ei_ctx, char * fmt, va_list args);
errorinfo_context * errorinfo_wrap(errorinfo_context * cause_ei_ctx, char * fmt, ...);

int errorinfo_appendmessage(errorinfo_context * ei_ctx, char * message);
int errorinfo_addinfoobject(errorinfo_context * ei_ctx, int type, void * data, void (*free_cb)(void *));

char * errorinfo_cstr(errorinfo_context * ei_ctx);

#endif /* ERRORINFO_H */

