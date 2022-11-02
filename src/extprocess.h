#ifndef EXTPROCESS_H
#define EXTPROCESS_H

#include <sys/types.h>
#include <stdarg.h>
#include "dynstring.h"

#ifdef __cplusplus
extern "C" {
#endif

struct extprocess_context {
	char ** arg;
	size_t arg_size;
	struct dynstring_context output;
	int wpipefd[2];
	int rpipefd[2];
	pid_t forkedpid;
	int exitcode;
};

struct extprocess_context * extprocess_create();
void extprocess_dispose(struct extprocess_context * ctx);
size_t extprocess_argumentcount(struct extprocess_context * ctx);
const char * extprocess_argumentget(struct extprocess_context * ctx, size_t idx);
int extprocess_argumentappend(struct extprocess_context * ctx, char * arg);
int extprocess_argumentappend_va(struct extprocess_context * ctx, va_list args);
int extprocess_argumentappendz(struct extprocess_context * ctx, ...);
int extprocess_execute(struct extprocess_context * ctx);
int extprocess_read(struct extprocess_context * ctx, int timeout_sec);
int extprocess_execute_getoutput(char * const args[], dynstring_context_t * output);

#ifdef __cplusplus
}
#endif

#endif /* EXTPROCESS_H */

