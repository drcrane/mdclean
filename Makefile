
CC?=gcc
CFLAGS=-g -Wno-unused-function -Wpedantic --std=c11 -O2 -Wall -Wno-unknown-pragmas
LDLIBS=-lfcgi

appsrc/%: appsrc/%.c
	$(CC) $(DEPFLAGS) $(LDFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@

