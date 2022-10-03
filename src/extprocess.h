#ifndef EXTPROCESS_H
#define EXTPROCESS_H

#include "dynstring.h"

#ifdef __cplusplus
extern "C" {
#endif

int extprocess_execute_getoutput(char * const args[], dynstring_context_t * output);

#ifdef __cplusplus
}
#endif

#endif /* EXTPROCESS_H */

