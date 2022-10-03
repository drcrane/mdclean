#ifndef QUOTEDSTRINGPARSER_H
#define QUOTEDSTRINGPARSER_H

#include "dynstring.h"

#ifdef __cplusplus
extern "C" {
#endif

int qsp_callbacker(char * input, void * arg, int(*cb)(void * arg, int idx, char * field));
int qsp_inplace(char * input, int max, ...);
int qsp_inplace_commandline(char * input, int max, ...);

size_t qsp_append_to_dynstring(dynstring_context_t * dynstring, char * buf, size_t len, char * delim);

#ifdef __cplusplus
};
#endif


#endif // QUOTEDSTRINGPARSER_H

