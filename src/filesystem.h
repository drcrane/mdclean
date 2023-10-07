#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stddef.h>

int filesystem_saveramtofile(const char * path, const void * data, const size_t size);
int filesystem_loadfiletoram(const char * path, void ** data_ptr, size_t * data_sz_ptr);

#endif // FILESYSTEM_H

