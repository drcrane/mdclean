#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include "mimeparse.h"

size_t strnlen(const char * str, size_t max_len);

char * strnstr(const char * haystack, const char * needle, size_t len) {
	int i;
	size_t needle_len;

	if (0 == (needle_len = strnlen(needle, len)))
		return (char *)haystack;

	for (i=0; i<=(int)(len-needle_len); i++)
	{
		if ((haystack[0] == needle[0]) &&
			(0 == strncmp(haystack, needle, needle_len)))
			return (char *)haystack;

		haystack++;
	}
	return NULL;
}

char * mimeparse_positionofany(const char * input, size_t input_len, const char * matches, size_t matches_len) {
	for(size_t i = 0; i < input_len; ++i) {
		for(size_t j = 0; j < matches_len; ++j) {
			if(input[i] == matches[j]) {
				return (char *)&input[i];
			}
		}
	}
	return NULL;
}

int mimeparse_getboundary(char ** res, const char * input, size_t input_len) {
	char * tokn_pos = strnstr(input, "boundary=", input_len);
	*res = NULL;
	if (tokn_pos == NULL) {
		return -1;
	}
    tokn_pos += 9;
    char * tokn_term = mimeparse_positionofany(tokn_pos, (input_len - (tokn_pos - input)) + 1, ";\r\n\0", 4);
	if (tokn_term == NULL) {
		return -2;
	}
    size_t tokn_len = tokn_term - tokn_pos;
	const char prfx[] = { '-', '-' };
    char * tokn = malloc(sizeof(prfx) + tokn_len + 1);
	if (tokn == NULL) {
		return -3;
	}
	memcpy(tokn, prfx, sizeof(prfx));
    memcpy(tokn + sizeof(prfx), tokn_pos, tokn_len);
    tokn[tokn_len - 1] = '\0';
	*res = tokn;
	return 0;
}

char ** mimeparse_findsubstrings(const char * input, size_t input_len, const char * substr, size_t * match_count) {
	int substr_len = strlen(substr);
	int max_occurrences = input_len / substr_len; // Max possible occurrences
	char ** locations;
	locations = (char**)malloc(max_occurrences * sizeof(char*));
	if (locations == NULL) {
		return NULL;
	}
	int occurrence_count = 0;
	for (int i = 0; i < input_len; i++) {
		if (input[i] == substr[0]) {
			int j;
			for (j = 1; j < substr_len; j++) {
				if (input[i + j] != substr[j]) {
					break;
				}
			}
			if (j == substr_len) {
				// Match found, store the location
				locations[occurrence_count] = (char *)(input + i);
				occurrence_count++;
			}
		}
	}
	*match_count = occurrence_count;
	return locations;
}

mimeparse_part * mimeparse_parseparts(const char * input, size_t input_len, char * boundary, size_t * part_count) {
	char ** boundaries;
	size_t boundary_count;
	size_t boundary_size;
	size_t i, part_idx;

	boundary_size = strlen(boundary);
	boundaries = mimeparse_findsubstrings(input, input_len, boundary, &boundary_count);
	if (boundaries == NULL) {
		return NULL;
	}
	mimeparse_part * mimeparts = malloc(sizeof(mimeparse_part) * boundary_count);
	if (mimeparts == NULL) {
		return NULL;
	}
	part_idx = 0;
	for (i = 0; i < boundary_count; i++) {
		if (i == 0) {
			fprintf(stdout, "%d: %d\n", (int)i, (int)(boundaries[i] - input));
		}
		if (i > 0) {
			size_t part_boundary_size = boundary_size;
			char * part_start = boundaries[i - 1] + part_boundary_size;
			// Each part will end with \r\n
			// and this will be removed here:
			size_t part_size = boundaries[i] - boundaries[i - 1] - part_boundary_size - 2;
			// End of headers is a blank line:
			char * content_start = strnstr(part_start, "\r\n\r\n", part_size);
			if (content_start) { content_start += 4; }
			// content_size is part size - header size.
			size_t content_size = part_size - (content_start - part_start);
			fprintf(stdout, "%d: %d\n", (int)i, (int)(boundaries[i] - boundaries[i - 1]));
			//fprintf(stdout, "----\n%.*s\n----\n", (int)(part_size), part_start);
			if (content_start == NULL) {
				fprintf(stdout, "This part was invalid.\n");
				continue;
			} else {
				fprintf(stdout, "----\n[%.*s]\n----\n", (int)content_size, content_start);
			}
			mimeparts[part_idx].header_start = part_start;
			mimeparts[part_idx].content_start = content_start;
			mimeparts[part_idx].content_size = content_size;
			part_idx ++;
		}
	}
	free(boundaries);
	*part_count = part_idx;
	return mimeparts;
}

