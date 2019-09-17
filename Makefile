DESTDIR ?=
PREFIX  ?= /usr/local

CFLAGS += -fPIC

lib_OBJ  = mustach.o mustach-json-c.o
tool_OBJ = mustach.o mustach-json-c.o mustach-tool.o

all: mustach mustach.so

install: mustach mustach.so
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib
	install -m0755 mustach    $(DESTDIR)$(PREFIX)/bin/
	install -m0755 mustach.so $(DESTDIR)$(PREFIX)/lib/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/mustach
	rm -f $(DESTDIR)$(PREFIX)/lib/mustach.so

mustach: $(tool_OBJ)
	$(CC) $(LDFLAGS) -o mustach $(tool_OBJ) $(LDLIBS) -ljson-c

mustach.so: $(lib_OBJ)
	$(CC) $(LDFLAGS) -shared -o mustach.so $(lib_OBJ) $(LDLIBS) -ljson-c

mustach.o:      mustach.h
mustach-json.o: mustach.h mustach-json-c.h
mustach-tool.o: mustach.h mustach-json-c.h

test: mustach
	@make -C test1 test
	@make -C test2 test
	@make -C test3 test
	@make -C test4 test
	@make -C test5 test
	@make -C test6 test

clean:
	rm -f mustach mustach.so *.o
	@make -C test1 clean
	@make -C test2 clean
	@make -C test3 clean
	@make -C test4 clean
	@make -C test5 clean
	@make -C test6 clean

.PHONY: test clean install uninstall
