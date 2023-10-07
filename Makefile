.PRECIOUS: %.o

MAKE?=make
CC?=gcc
CFLAGS=-g --std=c11 -O2 -Wall -Wpedantic -Wno-unused-function -Wno-unknown-pragmas -Isrc/
LDLIBS=-Llib/ -lapp

APPLIBS=-lfcgi

OBJDIR=obj/
DEPDIR=obj/
SRCDIR=src/

DEPS=$(wildcard $(OBJDIR)*.d) $(wildcard testsrc/*.d)

APPOBJ=obj/mimeparse.o
APPBIN=appbin/mdclean
TESTOBJ=obj/filesystem.o

def: $(APPBIN)

$(OBJDIR):
	mkdir $@

lib/:
	mkdir $@

appbin/:
	mkdir $@

testbin/:
	mkdir $@

lib/libapp.a: $(APPOBJ) | lib/
	ar -rcs $@ $(APPOBJ)

$(OBJDIR)%.o: $(SRCDIR)%.c | $(OBJDIR)
	$(CC) -c $(CFLAGS) -MMD -o $@ $<

appsrc/%.o: appsrc/%.c
	$(CC) -c $(CFLAGS) -MMD -o $@ $<

testsrc/%.o: testsrc/%.c
	$(CC) -c $(CFLAGS) -MMD -o $@ $<

appbin/%: appsrc/%.o lib/libapp.a | appbin/
	$(CC) $(DEPFLAGS) $(LDFLAGS) $(CFLAGS) $< $(LDLIBS) $(APPLIBS) -o $@

testbin/%: testsrc/%.o lib/libapp.a $(TESTOBJ) | testbin/
	$(CC) $(DEPFLAGS) $(LDFLAGS) $(CFLAGS) $(TESTOBJ) $< $(LDLIBS) -o $@

-include $(DEPS)

test: testbin/mimeparse_tests
	testbin/mimeparse_tests

clean:
	rm -f obj/*.o
	rm -f obj/*.d
	rm -f lib/*.a
	rm -f appsrc/*.o
	rm -f appsrc/*.d
	rm -f testsrc/*.o
	rm -f testsrc/*.d
	rm -f $(APPBIN)
	rm -f testbin/*

distclean:
	rm -rf obj/
	rm -rf lib/
	rm -rf appbin/
	rm -rf testbin/

