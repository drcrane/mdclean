.PRECIOUS: %.o

MAKE?=make
CC?=gcc
CFLAGS=-g --std=c11 -O2 -Wall -Wpedantic -Wno-unused-function -Wno-unknown-pragmas -Ideps/libgreen/include/ -Ideps/libgreen/src/ -Isrc/ -Iinc/
LDLIBS=-Llib/ -lapp -Ldeps/libgreen/lib/ -lgreen

APPLIBS=-lfcgi

OBJDIR=obj/
DEPDIR=obj/
SRCDIR=src/

DEPS=$(wildcard $(OBJDIR)*.d) $(wildcard testsrc/*.d)

APPOBJ=obj/formdecoder.o obj/contentdisposition.o obj/errorinfo.o obj/quotedstringparser.o obj/extprocess.o
APPBIN=appbin/mdclean
TESTOBJ=obj/mockfcgiapp.o

def: $(APPBIN)

appbin/:
	mkdir $@

testbin/:
	mkdir $@

deps/libgreen/lib/libgreen.a: deps/libgreen/
	$(MAKE) -C $< lib/libgreen.a

lib/libapp.a: $(APPOBJ)
	ar -rcs $@ $(APPOBJ)

$(OBJDIR)%.o: $(SRCDIR)%.c
	$(CC) -c $(CFLAGS) -MMD -o $@ $<

appsrc/%.o: appsrc/%.c
	$(CC) -c $(CFLAGS) -MMD -o $@ $<

testsrc/%.o: testsrc/%.c
	$(CC) -c $(CFLAGS) -MMD -o $@ $<

appbin/%: appsrc/%.o lib/libapp.a deps/libgreen/lib/libgreen.a | appbin/
	$(CC) $(DEPFLAGS) $(APPLIBS) $(LDFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@

testbin/%: testsrc/%.o lib/libapp.a deps/libgreen/lib/libgreen.a $(TESTOBJ) | testbin/
	$(CC) $(DEPFLAGS) $(LDFLAGS) $(CFLAGS) $(TESTOBJ) $< $(LDLIBS) -o $@

-include $(DEPS)

clean:
	rm -f obj/*.o
	rm -f obj/*.d
	rm -f lib/*.a
	rm -f $(APPBIN)
	rm -f testbin/*
#	$(MAKE) -C deps/libgreen/ clean

