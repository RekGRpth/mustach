DESTDIR ?=
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
LIBDIR  ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
SOVER = .0
SOVEREV = .0.99

#CFLAGS += -fPIC -Wall -Wextra
CFLAGS += -fPIC -Wall -Wextra -g
#LDLIBS += -ljson-c
LDLIBS += -ljansson

#lib_OBJ  = mustach.o mustach-wrap.o mustach-json-c.o
lib_OBJ  = mustach.o mustach-wrap.o mustach-jansson.o
tool_OBJ = $(lib_OBJ) mustach-tool.o
HEADERS  = mustach.h mustach-json-c.h

lib_LDFLAGS  += -shared
ifeq ($(shell uname),Darwin)
 lib_LDFLAGS += -install_name $(LIBDIR)/libmustach.so$(SOVEREV)
endif

all: mustach libmustach.so$(SOVEREV)

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)/mustach
	install -m0755 mustach       $(DESTDIR)$(BINDIR)/
	install -m0644 $(HEADERS)    $(DESTDIR)$(INCLUDEDIR)/mustach
	install -m0755 libmustach.so* $(DESTDIR)$(LIBDIR)/
	ln -sf libmustach.so$(SOVEREV) $(DESTDIR)$(LIBDIR)/libmustach.so$(SOVER)
	ln -sf libmustach.so$(SOVEREV) $(DESTDIR)$(LIBDIR)/libmustach.so


uninstall:
	rm -f $(DESTDIR)$(BINDIR)/mustach
	rm -f $(DESTDIR)$(LIBDIR)/libmustach.so*
	rm -rf $(DESTDIR)$(INCLUDEDIR)/mustach

mustach: $(tool_OBJ)
	$(CC) $(LDFLAGS) -o mustach $(tool_OBJ) $(LDLIBS)

libmustach.so$(SOVEREV): $(lib_OBJ)
	$(CC) $(LDFLAGS) $(lib_LDFLAGS) -o libmustach.so$(SOVEREV) $(lib_OBJ) $(LDLIBS)

mustach.o:            mustach.h
mustach-wrap.o:       mustach.h mustach-wrap.h
mustach-json-c.o:     mustach.h mustach-wrap.h mustach-json-c.h
mustach-jansson.o:    mustach.h mustach-wrap.h mustach-jansson.h
mustach-tool.o:       mustach.h mustach-json-c.h mustach-jansson.h

test: mustach
	@$(MAKE) -C test1 test
	@$(MAKE) -C test2 test
	@$(MAKE) -C test3 test
	@$(MAKE) -C test4 test
	@$(MAKE) -C test5 test
	@$(MAKE) -C test6 test

clean:
	rm -f mustach libmustach.so$(SOVEREV) *.o
	@$(MAKE) -C test1 clean
	@$(MAKE) -C test2 clean
	@$(MAKE) -C test3 clean
	@$(MAKE) -C test4 clean
	@$(MAKE) -C test5 clean
	@$(MAKE) -C test6 clean

.PHONY: test clean install uninstall
