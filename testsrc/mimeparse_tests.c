#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "mimeparse.h"
#include "filesystem.h"

static const char * mimeparse_findheaderend_test() {
	char * input = "Content-Disposition: form-data; name=\"file\";\r\n filename=\"testing_file.txt\"\r\n\r\n";
	size_t input_len = strlen(input);
	char * end_pos = mimeparse_findheaderend(input, input_len);
	if (end_pos == NULL) {
		return "mimeparse_findheaderend_test failed";
	}
	fprintf(stdout, "end of header : %d\n", (int)(end_pos - input));

	input = "Content-Disposition: form-data; name=\"file\";\r\n\tfilename=\"testing_file.txt\"\r\n\r\n";
	input_len = strlen(input);
	end_pos = mimeparse_findheaderend(input, input_len);
	if (end_pos == NULL) {
		return "mimeparse_findheaderend_test failed";
	}
	fprintf(stdout, "end of header : %d\n", (int)(end_pos - input));

	input = "Content-Disposition: form-data; name=\"file\"; filename=\"testing_file.txt\"\r\nContent-Type: text/plain\r\n\r\n";
	input_len = strlen(input);
	end_pos = mimeparse_findheaderend(input, input_len);
	if (end_pos == NULL) {
		return "mimeparse_findheaderend_test failed";
	}
	fprintf(stdout, "end of header : %d\n", (int)(end_pos - input));

	// This header has no end
	input = "Content-Disposition: form-data; name=\"file\"; filename=\"testing_file.txt\"";
	input_len = strlen(input);
	end_pos = mimeparse_findheaderend(input, input_len);
	if (end_pos != NULL) {
		return "mimeparse_findheaderend_test failed";
	}

	return NULL;
}

static const char * mimeparse_findheaderfield_test() {
	int res;
	char * input = "form-data; name=\"file\"; filename=\"testing_file.txt\"\r\nContent-Type: text/plain\r\n\r\n";
	size_t input_len = strlen(input);
	char * field_value;
	size_t field_len;
	res = mimeparse_findheaderfield(input, input_len, "name=", &field_value, &field_len);
	fprintf(stdout, "fieldname %.*s\n", (int)field_len, field_value);
	res = mimeparse_findheaderfield(input, input_len, "filename=", &field_value, &field_len);
	fprintf(stdout, "filename %.*s\n", (int)field_len, field_value);

	input = "form-data; name=tags_and_stuff; filename=testing_file.txt\r\nContent-Type: text/plain\r\n\r\n";
	res = mimeparse_findheaderfield(input, input_len, "name=", &field_value, &field_len);
	fprintf(stdout, "Field Name : %.*s\n", (int)field_len, field_value);
	res = mimeparse_findheaderfield(input, input_len, "filename=", &field_value, &field_len);
	fprintf(stdout, "File Name  : %.*s\n", (int)field_len, field_value);
	return NULL;
}

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

	char * testres = mimeparse_findheaderend_test();
	if (testres) {
		fprintf(stderr, "%s\n", testres);
		return 1;
	}

	testres = mimeparse_findheaderfield_test();
	if (testres) {
		fprintf(stderr, "%s\n", testres);
		return 1;
	}

	// Decode a header (in place)
	mimeparse_contentdispositionheader contentdisp =
	mimeparse_parsecontentdisposition(mimeparts[1].header_start, mimeparts[1].content_start - mimeparts[1].header_start);
	if (contentdisp.filename == NULL || contentdisp.fieldname == NULL) {
		fprintf(stderr, "filename or fieldname not found\n");
		return 1;
	}
	fprintf(stdout, "fieldname = %.*s\n", (int)contentdisp.fieldname_len, contentdisp.fieldname);
	fprintf(stdout, "filename = %.*s\n", (int)contentdisp.filename_len, contentdisp.filename);

	fprintf(stdout, "Everything seems to work properly\n");

	free(boundary);
	free(mimeparts);
	free(file_contents);

	return 0;
}

