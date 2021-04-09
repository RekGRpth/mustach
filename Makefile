# version
MAJOR := 1
MINOR := 0
REVIS := 0

# installation settings
DESTDIR ?=
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
LIBDIR  ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include

# initial settings
VERSION := $(MAJOR).$(MINOR).$(REVIS)
SOVER := .$(MAJOR)
SOVEREV := .$(MAJOR).$(MINOR)

HEADERS := mustach.h mustach-wrap.h
SPLITLIB := libmustach-core.so$(SOVEREV)
SINGLEOBJS := mustach.o mustach-wrap.o
SINGLEFLAGS :=
SINGLELIBS :=
TOOLOBJS :=
TOOLFLAGS :=
TOOLLIBS :=
TOOLDEP :=

# availability of JANSSON
ifneq ($(jansson),no)
 jansson_cflags := $(shell pkg-config --silence-errors --cflags jansson)
 jansson_libs := $(shell pkg-config --silence-errors --libs jansson)
 ifdef jansson_libs
  jansson := yes
  SPLITLIB += libmustach-jansson.so$(SOVEREV)
  SINGLEOBJS += mustach-jansson.o
  SINGLEFLAGS += ${jansson_cflags}
  SINGLELIBS += ${jansson_libs}
  TOOLOBJS := mustach-jansson.o
  TOOLFLAGS := ${jansson_cflags} -DTOOL=MUSTACH_TOOL_JANSSON
  TOOLLIBS := ${jansson_libs}
  TOOLDEP := mustach-jansson.h
 else
  ifeq ($(jansson),yes)
   $(error Can't find required library jansson)
  endif
  jansson := no
 endif
endif

# availability of CJSON
ifneq ($(cjson),no)
 cjson_cflags := $(shell pkg-config --silence-errors --cflags libcjson)
 cjson_libs := $(shell pkg-config --silence-errors --libs libcjson)
 ifdef cjson_libs
  cjson := yes
  SPLITLIB += libmustach-cjson.so$(SOVEREV)
  SINGLEOBJS += mustach-cjson.o
  SINGLEFLAGS += ${cjson_cflags}
  SINGLELIBS += ${cjson_libs}
  TOOLOBJS := mustach-cjson.o
  TOOLFLAGS := ${cjson_cflags} -DTOOL=MUSTACH_TOOL_CJSON
  TOOLLIBS := ${cjson_libs}
  TOOLDEP := mustach-cjson.h
 else
  ifeq ($(cjson),yes)
   $(error Can't find required library cjson)
  endif
  cjson := no
 endif
endif

# availability of JSON-C
ifneq ($(jsonc),no)
 jsonc_cflags := $(shell pkg-config --silence-errors --cflags json-c)
 jsonc_libs := $(shell pkg-config --silence-errors --libs json-c)
 ifdef jsonc_libs
  jsonc := yes
  SPLITLIB += libmustach-json-c.so$(SOVEREV)
  SINGLEOBJS += mustach-json-c.o
  SINGLEFLAGS += ${jsonc_cflags}
  SINGLELIBS += ${jsonc_libs}
  TOOLOBJS := mustach-json-c.o
  TOOLFLAGS := ${jsonc_cflags} -DTOOL=MUSTACH_TOOL_JSON_C
  TOOLLIBS := ${jsonc_libs}
  TOOLDEP := mustach-json-c.h
 else
  ifeq ($(jsonc),yes)
   $(error Can't find required library json-c)
  endif
  jsonc := no
 endif
endif

# mode of build
ifndef mode
 mode := single
endif

# initial settings
$(info jsonc   = ${jsonc})
$(info jansson = ${jansson})
$(info cjson   = ${cjson})
$(info mode    = ${mode})

# compute targets
ALL :=
ifdef TOOLOBJS
 ALL += mustach
endif
ifeq (${mode},split)
 ALL += ${SPLITLIB}
else ifeq (${mode},single)
 ALL += libmustach.so$(SOVEREV)
else ifeq (${mode},all)
 ALL += libmustach.so$(SOVEREV) ${SPLITLIB}
else
 $(error Unknown mode $(mode))
endif

all: ${ALL}

# settings

CFLAGS += -fPIC -Wall -Wextra -DVERSION=${VERSION}

ifeq ($(shell uname),Darwin)
 darwin_single = -install_name $(LIBDIR)/libmustach.so$(SOVEREV)
 darwin_core = -install_name $(LIBDIR)/libmustach-core.so$(SOVEREV)
 darwin_cjson = -install_name $(LIBDIR)/libmustach-cjson.so$(SOVEREV)
 darwin_jsonc = -install_name $(LIBDIR)/libmustach-json-c.so$(SOVEREV)
 darwin_jansson = -install_name $(LIBDIR)/libmustach-jansson.so$(SOVEREV)
endif

# targets

mustach: mustach-tool.o mustach.o mustach-wrap.o $(TOOLOBJS)
	$(CC) $(LDFLAGS) $(TOOLFLAGS) -o mustach $^ $(TOOLLIBS)

libmustach.so$(SOVEREV): $(SINGLEOBJS)
	$(CC) -shared $(LDFLAGS) $(darwin_single) -o $@ $^  $(SINGLELIBS)

libmustach-core.so$(SOVEREV): mustach.o mustach-wrap.o
	$(CC) -shared $(LDFLAGS) $(darwin_core) -o $@ $^ $(lib_OBJ)

libmustach-cjson.so$(SOVEREV): mustach.o mustach-wrap.o mustach-cjson.o
	$(CC) -shared $(LDFLAGS) $(darwin_cjson) -o $@ $^ $(cjson_libs)

libmustach-json-c.so$(SOVEREV): mustach.o mustach-wrap.o mustach-json-c.o
	$(CC) -shared $(LDFLAGS) $(darwin_jsonc) -o $@ $^ $(jsonc_libs)

libmustach-jansson.so$(SOVEREV): mustach.o mustach-wrap.o mustach-jansson.o
	$(CC) -shared $(LDFLAGS) $(darwin_jansson) -o $@ $^ $(jansson_libs)

# objects

mustach.o: mustach.c mustach.h
	$(CC) -c $(CFLAGS) -o $@ $<

mustach-wrap.o: mustach-wrap.c mustach.h mustach-wrap.h
	$(CC) -c $(CFLAGS) -o $@ $<

mustach-tool.o: mustach-tool.c mustach.h mustach-json-c.h $(TOOLDEP)
	$(CC) -c $(CFLAGS) $(TOOLFLAGS) -o $@ $<

mustach-cjson.o: mustach-cjson.c mustach.h mustach-wrap.h mustach-cjson.h
	$(CC) -c $(CFLAGS) $(cjson_cflags) -o $@ $<

mustach-json-c.o: mustach-json-c.c mustach.h mustach-wrap.h mustach-json-c.h
	$(CC) -c $(CFLAGS) $(jsonc_cflags) -o $@ $<

mustach-jansson.o: mustach-jansson.c mustach.h mustach-wrap.h mustach-jansson.h
	$(CC) -c $(CFLAGS) $(jansson_cflags) -o $@ $<


# installing
install: all
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)/mustach
	install -m0755 mustach       $(DESTDIR)$(BINDIR)/
	install -m0644 $(HEADERS)    $(DESTDIR)$(INCLUDEDIR)/mustach
	for x in libmustach*.so*; do \
		install -m0755 $$x $(DESTDIR)$(LIBDIR)/ ;\
		ln -sf $$x $(DESTDIR)$(LIBDIR)/$${x%.so.*}.so$(SOVER) ;\
		ln -sf $$x $(DESTDIR)$(LIBDIR)/$${x%.so.*}.so ;\
	done

# deinstalling
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/mustach
	rm -f $(DESTDIR)$(LIBDIR)/libmustach*.so*
	rm -rf $(DESTDIR)$(INCLUDEDIR)/mustach

# testing
test: mustach
	@$(MAKE) -C test1 test
	@$(MAKE) -C test2 test
	@$(MAKE) -C test3 test
	@$(MAKE) -C test4 test
	@$(MAKE) -C test5 test
	@$(MAKE) -C test6 test

#cleaning
clean:
	rm -f mustach libmustach*.so* *.o
	rm -rf *.gcno *.gcda coverage.info gcov-latest
	@$(MAKE) -C test1 clean
	@$(MAKE) -C test2 clean
	@$(MAKE) -C test3 clean
	@$(MAKE) -C test4 clean
	@$(MAKE) -C test5 clean
	@$(MAKE) -C test6 clean

.PHONY: test clean install uninstall
