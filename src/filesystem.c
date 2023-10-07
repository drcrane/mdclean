#include "filesystem.h"
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

int filesystem_saveramtofile(const char * path, const void * data, const size_t size) {
	int rc, fd;
	ssize_t wrsz;
	fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd == -1) {
		return -1;
	}
	wrsz = write(fd, data, size);
	rc = ftruncate(fd, size);
	close(fd);
	if (wrsz != size) {
		unlink(path);
		return -1;
	}
	if (rc) {
		return -1;
	}
	return 0;
}

int filesystem_loadfiletoram(const char * path, void ** data_ptr, size_t * data_sz_ptr) {
	int rc, fd = -1;
	void * data;
	ssize_t data_sz;
	struct stat file_stat;
	rc = lstat(path, &file_stat);
	if (rc == -1 || S_ISDIR(file_stat.st_mode)) {
		return -1;
	}
	data = malloc(file_stat.st_size + 1);
	if (data == NULL) {
		return -1;
	}
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		goto error;
	}
	data_sz = read(fd, data, file_stat.st_size);
	if (data_sz == -1) {
		goto error;
	}
	*(((char *)data) + data_sz) = '\0';
	close(fd);
	*data_ptr = data;
	*data_sz_ptr = (size_t)data_sz;
	return 0;
error:
	if (data) {
		free(data);
	}
	if (fd != -1) {
		close(fd);
	}
	return -1;
}

