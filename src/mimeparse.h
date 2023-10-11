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

typedef struct mimeparse_contentdispositionheader {
	char * filename;
	size_t filename_len;
	char * fieldname;
	size_t fieldname_len;
} mimeparse_contentdispositionheader;

char * mimeparse_findheaderend(const char * input, size_t input_len);
int mimeparse_findheaderfield(const char * input, size_t input_len, const char * field_name, char ** starts_at, size_t * value_len);
mimeparse_contentdispositionheader mimeparse_parsecontentdisposition(const char * input, size_t input_len);

#endif // MIMEPARSE_H
