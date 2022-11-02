#define _POSIX_C_SOURCE 200809

#include <stddef.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>
#include "dbg.h"
#include "extprocess.h"
#include "dynstring.h"

#define EXTPROCESS_CONTEXT_DEFAULTARGSCOUNT 8
#define EXTPROCESS_CONTEXT_GROWARGSBY 8

struct extprocess_context * extprocess_create() {
	struct extprocess_context * ctx;
	ctx = malloc(sizeof(struct extprocess_context));
	if (ctx == NULL) {
		goto finished;
	}
	ctx->arg = NULL;
	ctx->arg_size = 0;
	dynstring_initialise(&ctx->output, 8192);
finished:
	return ctx;
}

void extprocess_dispose(struct extprocess_context * ctx) {
	if (ctx->arg) {
		char * arg = NULL;
		size_t idx = 0;
		do {
			arg = ctx->arg[idx];
			if (arg == NULL) {
				break;
			}
			free(arg);
			++ idx;
		} while (idx < ctx->arg_size);
		free(ctx->arg);
	}
	dynstring_free(&ctx->output);
	free(ctx);
}

size_t extprocess_argumentcount(struct extprocess_context * ctx) {
	size_t arg_count;
	if (ctx->arg == NULL) {
		return 0;
	}
	arg_count = 0;
	while (ctx->arg[arg_count]) {
		++ arg_count;
	}
	return arg_count;
}

const char * extprocess_argumentget(struct extprocess_context * ctx, size_t idx) {
	return ctx->arg[idx];
}

int extprocess_argumentappend(struct extprocess_context * ctx, char * arg) {
	size_t arg_count;
	size_t arg_len;
	if (ctx->arg == NULL) {
		ctx->arg = malloc(EXTPROCESS_CONTEXT_DEFAULTARGSCOUNT * sizeof(*ctx->arg));
		if (ctx->arg == NULL) {
			return -1;
		}
		ctx->arg[0] = NULL;
		ctx->arg_size = EXTPROCESS_CONTEXT_DEFAULTARGSCOUNT;
	}
	arg_count = extprocess_argumentcount(ctx);
	if (arg_count >= (ctx->arg_size - 1)) {
		char ** new_arg;
		new_arg = realloc(ctx->arg, (ctx->arg_size + EXTPROCESS_CONTEXT_GROWARGSBY) * sizeof(*ctx->arg));
		if (new_arg == NULL) {
			return -1;
		}
		ctx->arg = new_arg;
		ctx->arg_size = (ctx->arg_size + EXTPROCESS_CONTEXT_GROWARGSBY);
	}
	arg_len = strlen(arg) + 1;
	ctx->arg[arg_count] = malloc(arg_len);
	if (ctx->arg[arg_count] == NULL) {
		return -1;
	}
	memcpy(ctx->arg[arg_count], arg, arg_len);
	arg_count = arg_count + 1;
	ctx->arg[arg_count] = NULL;
	return 0;
}

int extprocess_argumentappend_va(struct extprocess_context * ctx, va_list args) {
	char * str;
	int rc;
	str = va_arg(args, char *);
	while (str != NULL) {
		rc = extprocess_argumentappend(ctx, str);
		if (rc) { break; }
		str = va_arg(args, char *);
	}
	return rc;
}

int extprocess_argumentappendz(struct extprocess_context * ctx, ...) {
	int rc;
	va_list args;
	va_start(args, ctx);
	rc = extprocess_argumentappend_va(ctx, args);
	va_end(args);
	return rc;
}

int extprocess_execute_getoutput(char * const args[], dynstring_context_t * output) {
	int wpipefd[2];
	int rpipefd[2];
	int res;
	pid_t forked;
	int wstatus = 0;

	wpipefd[0] = -1;
	wpipefd[1] = -1;
	rpipefd[0] = -1;
	rpipefd[1] = -1;

	res = pipe(wpipefd);
	check_debug(res >= 0, "Error creating wpipe");
	res = pipe(rpipefd);
	check_debug(res >= 0, "Error creating rpipe");

	forked = fork();
	check_debug(forked >= 0, "Failed to fork child");
	if (forked == 0) {
		// In Child
		// Now we have forked we cannot use ANY C library functions!
		close(wpipefd[1]);
		close(rpipefd[0]);
		wpipefd[1] = -1;
		rpipefd[0] = -1;
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		if (dup2(wpipefd[0], STDIN_FILENO) == -1) { goto child_error; }
		if (dup2(rpipefd[1], STDOUT_FILENO) == -1) { goto child_error; }
		close(wpipefd[0]);
		close(rpipefd[1]);
		// This either fails or replaces the current process image and does not return
		res = execv(args[0], args);
		fprintf(stderr, "Failed execting %s (%d) errno %d\n", args[0], res, errno);
child_error:
		exit(EXIT_FAILURE);
	} else {
		size_t pos;
		size_t bytes_read;
		size_t buflen;
		char * buf;
		fd_set read_fds;
		fd_set write_fds;
		struct timeval timeout;
		int timeout_count;
		pid_t pidchanged;

		close(wpipefd[0]);
		close(rpipefd[1]);
		//pos = 0;
		//buflen = 8192;
		//buf = malloc(buflen);
		//if (buf == NULL) { fprintf(stderr, "Error allocating memory\n"); goto error; }
		bytes_read = 0;
		timeout_count = 120;
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		do {
			FD_SET(rpipefd[0], &read_fds);
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			res = select(rpipefd[0] + 1, &read_fds, &write_fds, NULL, &timeout);
			if (res == 0) {
				timeout_count --;
				fprintf(stderr, "timeout %d bytes read %li\n", timeout_count, bytes_read);
			} else
			if (res > 0) {
				if (FD_ISSET(rpipefd[0], &read_fds)) {
					buflen = dynstring_freespace(output);
					if (buflen < 1024) {
						res = dynstring_extend(output, 4096);
						if (res) {
							fprintf(stderr, "error extending output buffer\n");
							goto error;
						}
						buflen = dynstring_freespace(output);
					}
					pos = dynstring_length(output);
					buf = output->buf;
					res = read(rpipefd[0], buf + pos, buflen);
					if (res < 0) {
						fprintf(stderr, "res = %d, errno %d\n", res, errno);
					} else
					if (res == 0) {
						fprintf(stderr, "res was 0!\n");
						break;
					} else {
						bytes_read += res;
						pos += res;
						output->pos += res;
					}
					if (bytes_read > 8192) {
						fprintf(stderr, "read too much, exiting\n");
						break;
					}
				}
				if (FD_ISSET(wpipefd[1], &write_fds)) {
					//res = write(wpipefd[1], buf + pos, buflen - pos);
				}
			} else {
				goto error;
			}
			FD_CLR(rpipefd[0], &read_fds);
		} while (timeout_count);
		close(rpipefd[0]);
		rpipefd[0] = -1;
		close(wpipefd[1]);
		wpipefd[1] = -1;
		fprintf(stderr, "forked pid %i\n", forked);
		timeout_count = 10;
		do {
			pidchanged = waitpid(forked, &wstatus, WNOHANG);
			if (pidchanged == -1) {
				// error (something wrong with waitpid()
				fprintf(stderr, "waitpid() == -1\n");
				goto error;
			} else
			if (pidchanged == 0) {
				// PID exists but state has not changed
				fprintf(stderr, "waitpid() == 0 (pid state not changed)\n");
			} else {
				if (WIFEXITED(wstatus)) {
					fprintf(stderr, "pid %i exited with status %i\n", pidchanged, WEXITSTATUS(wstatus));
					wstatus = WEXITSTATUS(wstatus);
					break;
				} else
				if (WIFSIGNALED(wstatus)) {
					fprintf(stderr, "pid %i was signalled by %i\n", pidchanged, WTERMSIG(wstatus));
					wstatus = -1;
					break;
				} else {
					fprintf(stderr, "pid %i not exited\n", pidchanged);
				}
			}
			if (timeout_count == 5) {
				fprintf(stderr, "PID %i NOT Exited, killing it...\n", forked);
				kill(forked, SIGKILL);
			}
			sleep (1);
		} while (timeout_count --);
	}

	return wstatus;
error:
	if (wpipefd[0] != -1) { close(wpipefd[0]); }
	if (wpipefd[1] != -1) { close(wpipefd[1]); }
	if (rpipefd[0] != -1) { close(rpipefd[0]); }
	if (rpipefd[1] != -1) { close(rpipefd[1]); }
	return -1;
}

