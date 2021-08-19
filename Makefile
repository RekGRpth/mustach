# version
MAJOR := 1
MINOR := 1
REVIS := 1

# installation settings
DESTDIR ?=
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
LIBDIR  ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
MANDIR  ?= $(PREFIX)/share/man
PKGDIR  ?= $(LIBDIR)/pkgconfig

# Tools (sed must be GNU sed)
SED ?= sed

# initial settings
VERSION := $(MAJOR).$(MINOR).$(REVIS)
SOVER := .$(MAJOR)
SOVEREV := .$(MAJOR).$(MINOR)

HEADERS := mustach.h mustach-wrap.h
SPLITLIB := libmustach-core.so$(SOVEREV)
SPLITPC := libmustach-core.pc
COREOBJS := mustach.o mustach-wrap.o
SINGLEOBJS := $(COREOBJS)
SINGLEFLAGS :=
SINGLELIBS :=
ALL := manuals

# availability of CJSON
ifneq ($(cjson),no)
 cjson_cflags := $(shell pkg-config --silence-errors --cflags libcjson)
 cjson_libs := $(shell pkg-config --silence-errors --libs libcjson)
 ifdef cjson_libs
  cjson := yes
  tool ?= cjson
  HEADERS += mustach-cjson.h
  SPLITLIB += libmustach-cjson.so$(SOVEREV)
  SPLITPC += libmustach-cjson.pc
  SINGLEOBJS += mustach-cjson.o
  SINGLEFLAGS += ${cjson_cflags}
  SINGLELIBS += ${cjson_libs}
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
  tool ?= jsonc
  HEADERS += mustach-json-c.h
  SPLITLIB += libmustach-json-c.so$(SOVEREV)
  SPLITPC += libmustach-json-c.pc
  SINGLEOBJS += mustach-json-c.o
  SINGLEFLAGS += ${jsonc_cflags}
  SINGLELIBS += ${jsonc_libs}
 else
  ifeq ($(jsonc),yes)
   $(error Can't find required library json-c)
  endif
  jsonc := no
 endif
endif

# availability of JANSSON
ifneq ($(jansson),no)
 jansson_cflags := $(shell pkg-config --silence-errors --cflags jansson)
 jansson_libs := $(shell pkg-config --silence-errors --libs jansson)
 ifdef jansson_libs
  jansson := yes
  tool ?= jansson
  HEADERS += mustach-jansson.h
  SPLITLIB += libmustach-jansson.so$(SOVEREV)
  SPLITPC += libmustach-jansson.pc
  SINGLEOBJS += mustach-jansson.o
  SINGLEFLAGS += ${jansson_cflags}
  SINGLELIBS += ${jansson_libs}
 else
  ifeq ($(jansson),yes)
   $(error Can't find required library jansson)
  endif
  jansson := no
 endif
endif

# tool
TOOLOBJS = mustach-tool.o $(COREOBJS)
tool ?= none
ifneq ($(tool),none)
  ifeq ($(tool),cjson)
    TOOLOBJS += mustach-cjson.o
    TOOLFLAGS := ${cjson_cflags} -DTOOL=MUSTACH_TOOL_CJSON
    TOOLLIBS := ${cjson_libs}
    TOOLDEP := mustach-cjson.h
  else ifeq ($(tool),jsonc)
    TOOLOBJS += mustach-json-c.o
    TOOLFLAGS := ${jsonc_cflags} -DTOOL=MUSTACH_TOOL_JSON_C
    TOOLLIBS := ${jsonc_libs}
    TOOLDEP := mustach-json-c.h
  else ifeq ($(tool),jansson)
    TOOLOBJS += mustach-jansson.o
    TOOLFLAGS := ${jansson_cflags} -DTOOL=MUSTACH_TOOL_JANSSON
    TOOLLIBS := ${jansson_libs}
    TOOLDEP := mustach-jansson.h
  else
   $(error Unknown library $(tool) for tool)
  endif
  ifneq ($($(tool)),yes)
    $(error No library found for tool $(tool))
  endif
  ALL += mustach
endif

# compute targets
libs ?= all
ifeq (${libs},split)
 ALL += ${SPLITLIB} ${SPLITPC}
else ifeq (${libs},single)
 ALL += libmustach.so$(SOVEREV) libmustach.pc
else ifeq (${libs},all)
 ALL += libmustach.so$(SOVEREV) libmustach.pc ${SPLITLIB} ${SPLITPC}
else ifneq (${libs},none)
 $(error Unknown libs $(libs))
endif

# display target
$(info tool    = ${tool})
$(info libs    = ${libs})
$(info jsonc   = ${jsonc})
$(info jansson = ${jansson})
$(info cjson   = ${cjson})

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

.PHONY: all
all: ${ALL}

mustach: $(TOOLOBJS)
	$(CC) $(LDFLAGS) $(TOOLFLAGS) -o mustach $(TOOLOBJS) $(TOOLLIBS)

libmustach.so$(SOVEREV): $(SINGLEOBJS)
	$(CC) -shared $(LDFLAGS) $(darwin_single) -o $@ $^ $(SINGLELIBS)

libmustach-core.so$(SOVEREV): $(COREOBJS)
	$(CC) -shared $(LDFLAGS) $(darwin_core) -o $@ $(COREOBJS) $(lib_OBJ)

libmustach-cjson.so$(SOVEREV): $(COREOBJS) mustach-cjson.o
	$(CC) -shared $(LDFLAGS) $(darwin_cjson) -o $@ $^ $(cjson_libs)

libmustach-json-c.so$(SOVEREV): $(COREOBJS) mustach-json-c.o
	$(CC) -shared $(LDFLAGS) $(darwin_jsonc) -o $@ $^ $(jsonc_libs)

libmustach-jansson.so$(SOVEREV): $(COREOBJS) mustach-jansson.o
	$(CC) -shared $(LDFLAGS) $(darwin_jansson) -o $@ $^ $(jansson_libs)

# pkgconfigs

%.pc: pkgcfgs
	$(SED) -E '/^==.*==$$/{h;d};x;/==$@==/{x;s/VERSION/$(VERSION)/;p;d};x;d' $< > $@

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
.PHONY: install
install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m0755 mustach       $(DESTDIR)$(BINDIR)/
	install -d $(DESTDIR)$(INCLUDEDIR)/mustach
	install -m0644 $(HEADERS)    $(DESTDIR)$(INCLUDEDIR)/mustach
	install -d $(DESTDIR)$(LIBDIR)
	for x in libmustach*.so$(SOVEREV); do \
		install -m0755 $$x $(DESTDIR)$(LIBDIR)/ ;\
		ln -sf $$x $(DESTDIR)$(LIBDIR)/$${x%.so.*}.so$(SOVER) ;\
		ln -sf $$x $(DESTDIR)$(LIBDIR)/$${x%.so.*}.so ;\
	done
	install -d $(DESTDIR)/$(PKGDIR)
	install -m0644 libmustach*.pc $(DESTDIR)/$(PKGDIR)
	install -d $(DESTDIR)/$(MANDIR)/man1
	install -m0644 mustach.1.gz $(DESTDIR)/$(MANDIR)/man1

# deinstalling
.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/mustach
	rm -f $(DESTDIR)$(LIBDIR)/libmustach*.so*
	rm -rf $(DESTDIR)$(INCLUDEDIR)/mustach

# testing
.PHONY: test
test:
	@$(MAKE) -C test1 test
	@$(MAKE) -C test2 test
	@$(MAKE) -C test3 test
	@$(MAKE) -C test4 test
	@$(MAKE) -C test5 test
	@$(MAKE) -C test6 test

#cleaning
.PHONY: clean
clean:
	rm -f mustach libmustach*.so* *.o *.pc
	rm -rf *.gcno *.gcda coverage.info gcov-latest
	@$(MAKE) -C test1 clean
	@$(MAKE) -C test2 clean
	@$(MAKE) -C test3 clean
	@$(MAKE) -C test4 clean
	@$(MAKE) -C test5 clean
	@$(MAKE) -C test6 clean

# manpage
.PHONY: manual
manuals: mustach.1.gz

mustach.1.gz: mustach.1.scd
	if which scdoc >/dev/null 2>&1; then scdoc < mustach.1.scd | gzip > mustach.1.gz; fi

