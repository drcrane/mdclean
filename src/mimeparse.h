#ifndef MIMEPARSE_H
#define MIMEPARSE_H

#include <stddef.h>

typedef struct mimeparse_part {
	char * header_start;
	char * content_start;
	size_t content_size;
} mimeparse_part;

char * strnstr(const char * haystack, const char * needle, size_t len);
int mimeparse_getboundary(char ** res, const char * input, size_t input_len);
char * mimeparse_positionofany(const char * input, size_t input_len, const char * matches, size_t matches_len);
char ** mimeparse_findsubstrings(const char * input, size_t input_len, const char * boundary, size_t * match_count);
mimeparse_part * mimeparse_parseparts(const char * input, size_t input_len, char * boundary, size_t * part_count);

#endif // MIMEPARSE_H
