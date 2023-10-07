#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "mimeparse.h"
#include "filesystem.h"

int main(int argc, char *argv[]) {
	char * content_type = "multipart/form-data; boundary=---------------------------350468835916051811984031396419";
	char * boundary = NULL;
	size_t boundary_size = 0;
	char * file_contents = NULL;
	size_t file_size;
	int res;
	size_t boundary_count = 0;
	char ** boundaries;
	mimeparse_part * mimeparts;
	size_t mimepart_count;
	char * testing_contents;
	size_t testing_size;

	res = mimeparse_getboundary(&boundary, content_type, strlen(content_type));
	if (res != 0) {
		fprintf(stderr, "Boundary parsing failed: %d\n", res);
		return 1;
	}
	boundary_size = strlen(content_type);
	fprintf(stdout, "Detected Boundary: %s (%d)\n", boundary, (int)boundary_size);

	res = filesystem_loadfiletoram("testdata/simplepost.mime", (void *)&file_contents, &file_size);

	if (res != 0) {
		fprintf(stderr, "Read file failed\n");
		return 1;
	}

	fprintf(stdout, "Open: %d Size: %d\n", res, (int)file_size);

	boundaries = mimeparse_findsubstrings(file_contents, file_size, boundary, &boundary_count);
	if (boundaries == NULL) {
		fprintf(stderr, "Boundaries not found\n");
		return 1;
	}
	free(boundaries);

	fprintf(stdout, "Boundaries Found: %d\n", (int)boundary_count);

	mimeparts = mimeparse_parseparts(file_contents, file_size, boundary, &mimepart_count);
	if (mimeparts == NULL) {
		fprintf(stderr, "No parts found\n");
		return 1;
	}

	res = filesystem_loadfiletoram("testdata/testing_file.txt", (void *)&testing_contents, &testing_size);
	if (res != 0) {
		fprintf(stderr, "Cannot load original testing file\n");
		return 1;
	}
	if (mimeparts[1].content_size != testing_size) {
		fprintf(stdout, "Size not the same\n");
		return 1;
	}
	res = memcmp(mimeparts[1].content_start, testing_contents, testing_size);
	if (res != 0) {
		fprintf(stdout, "Decoded contents not the same as original contents\n");
		return 1;
	}
	fprintf(stdout, "Everything seems to work properly\n");

	free(boundary);
	free(mimeparts);
	free(file_contents);

	return 0;
}

