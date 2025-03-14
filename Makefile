# version
MAJOR := 2
MINOR := 0
REVIS := 0alpha

# Mustache spec
VSPEC := v1.4.2

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
INSTALL ?= install

# initial settings
VERSION := $(MAJOR).$(MINOR).$(REVIS)
SOVER := .$(MAJOR)
SOVEREV := .$(MAJOR).$(MINOR)

HEADERS := mini-mustach.h mustach-helpers.h mustach2.h mustach.h mustach-wrap.h
SPLITLIB := libmustach-core.so$(SOVEREV)
SPLITPC := libmustach-core.pc
COREOBJS := mini-mustach.o mustach-helpers.o mustach2.o mustach.o mustach-wrap.o
SINGLEOBJS := $(COREOBJS)
SINGLEFLAGS :=
SINGLELIBS :=
TESTSPECS :=
ALL := manuals
TESTPARENT ?= 0

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
  TESTSPECS += test-specs/test-specs-cjson
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
  TESTSPECS += test-specs/test-specs-json-c
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
  TESTSPECS += test-specs/test-specs-jansson
 else
  ifeq ($(jansson),yes)
   $(error Can't find required library jansson)
  endif
  jansson := no
 endif
endif

# tool
TOOLOBJS = $(COREOBJS)
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
  ALL += mustach mustachs
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

EFLAGS = -fPIC -Wall -Wextra -DVERSION=${VERSION}

ifeq ($(shell uname),Darwin)
 LDFLAGS_single  += -install_name $(LIBDIR)/libmustach.so$(SOVEREV)
 LDFLAGS_core    += -install_name $(LIBDIR)/libmustach-core.so$(SOVEREV)
 LDFLAGS_cjson   += -install_name $(LIBDIR)/libmustach-cjson.so$(SOVEREV)
 LDFLAGS_jsonc   += -install_name $(LIBDIR)/libmustach-json-c.so$(SOVEREV)
 LDFLAGS_jansson += -install_name $(LIBDIR)/libmustach-jansson.so$(SOVEREV)
else
 LDFLAGS_single  += -Wl,-soname,libmustach.so$(SOVER)
 LDFLAGS_core    += -Wl,-soname,libmustach-core.so$(SOVER)
 LDFLAGS_cjson   += -Wl,-soname,libmustach-cjson.so$(SOVER)
 LDFLAGS_jsonc   += -Wl,-soname,libmustach-json-c.so$(SOVER)
 LDFLAGS_jansson += -Wl,-soname,libmustach-jansson.so$(SOVER)
endif

# targets

.PHONY: all
all: ${ALL}

mustach: $(TOOLOBJS) mustach-tool.o
	$(CC) $(LDFLAGS) $(TOOLFLAGS) -o mustach $^ $(TOOLLIBS)

mustachs: $(TOOLOBJS) mustachs.o
	$(CC) $(LDFLAGS) $(TOOLFLAGS) -o mustachs $^ $(TOOLLIBS)

libmustach.so$(SOVEREV): $(SINGLEOBJS)
	$(CC) -shared $(LDFLAGS) $(LDFLAGS_single) -o $@ $^ $(SINGLELIBS)

libmustach-core.so$(SOVEREV): $(COREOBJS)
	$(CC) -shared $(LDFLAGS) $(LDFLAGS_core) -o $@ $(COREOBJS) $(lib_OBJ)

libmustach-cjson.so$(SOVEREV): $(COREOBJS) mustach-cjson.o
	$(CC) -shared $(LDFLAGS) $(LDFLAGS_cjson) -o $@ $^ $(cjson_libs)

libmustach-json-c.so$(SOVEREV): $(COREOBJS) mustach-json-c.o
	$(CC) -shared $(LDFLAGS) $(LDFLAGS_jsonc) -o $@ $^ $(jsonc_libs)

libmustach-jansson.so$(SOVEREV): $(COREOBJS) mustach-jansson.o
	$(CC) -shared $(LDFLAGS) $(LDFLAGS_jansson) -o $@ $^ $(jansson_libs)

# pkgconfigs

%.pc: pkgcfgs
	$(SED) -E '/^==.*==$$/{h;d};x;/==$@==/{x;s/VERSION/$(VERSION)/;p;d};x;d' $< > $@

# objects

mini-mustach.o: mini-mustach.c mini-mustach.h
	$(CC) -c $(EFLAGS) $(CFLAGS) -o $@ $<

mustach.o: mustach.c mustach.h mini-mustach.h mustach2.h mustach-helpers.h
	$(CC) -c $(EFLAGS) $(CFLAGS) -o $@ $<

mustach2.o: mustach2.c mini-mustach.h mustach2.h mustach-helpers.h
	$(CC) -c $(EFLAGS) $(CFLAGS) -o $@ $<

mustach-helpers.o: mustach-helpers.c mustach-helpers.h mini-mustach.h mustach2.h
	$(CC) -c $(EFLAGS) $(CFLAGS) -o $@ $<

mustach-wrap.o: mustach-wrap.c mini-mustach.h mustach2.h mustach.h mustach-wrap.h
	$(CC) -c $(EFLAGS) $(CFLAGS) -o $@ $<

mustach-tool.o: mustach-tool.c mini-mustach.h mustach2.h mustach-wrap.h $(TOOLDEP)
	$(CC) -c $(EFLAGS) $(CFLAGS) $(TOOLFLAGS) -o $@ $<

mustach-cjson.o: mustach-cjson.c mini-mustach.h mustach2.h mustach-wrap.h mustach-cjson.h
	$(CC) -c $(EFLAGS) $(CFLAGS) $(cjson_cflags) -o $@ $<

mustach-json-c.o: mustach-json-c.c mini-mustach.h mustach2.h mustach-wrap.h mustach-json-c.h
	$(CC) -c $(EFLAGS) $(CFLAGS) $(jsonc_cflags) -o $@ $<

mustach-jansson.o: mustach-jansson.c mini-mustach.h mustach2.h mustach-wrap.h mustach-jansson.h
	$(CC) -c $(EFLAGS) $(CFLAGS) $(jansson_cflags) -o $@ $<

mustachs.o: mustachs.c mini-mustach.h mustach2.h mustach-wrap.h $(TOOLDEP)
	$(CC) -c $(EFLAGS) $(CFLAGS) $(TOOLFLAGS) -o $@ $<

# installing
.PHONY: install
install: all
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	if test "${tool}" != "none"; then \
		$(INSTALL) -m0755 mustach $(DESTDIR)$(BINDIR)/; \
	fi
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR)/mustach
	$(INSTALL) -m0644 $(HEADERS)    $(DESTDIR)$(INCLUDEDIR)/mustach
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	for x in libmustach*.so$(SOVEREV); do \
		$(INSTALL) -m0755 $$x $(DESTDIR)$(LIBDIR)/ ;\
		ln -sf $$x $(DESTDIR)$(LIBDIR)/$${x%.so.*}.so$(SOVER) ;\
		ln -sf $$x $(DESTDIR)$(LIBDIR)/$${x%.so.*}.so ;\
	done
	$(INSTALL) -d $(DESTDIR)/$(PKGDIR)
	$(INSTALL) -m0644 libmustach*.pc $(DESTDIR)/$(PKGDIR)
	$(INSTALL) -d $(DESTDIR)/$(MANDIR)/man1
	$(INSTALL) -m0644 mustach.1.gz $(DESTDIR)/$(MANDIR)/man1

# deinstalling
.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/mustach
	rm -f $(DESTDIR)$(LIBDIR)/libmustach*.so*
	rm -rf $(DESTDIR)$(INCLUDEDIR)/mustach

.PHONY: test

test: mustach
	@$(MAKE) -C tests test VSPEC="$(VSPEC)" \
		TESTSPECS="$(TESTSPECS)"  TESTPARENT="$(TESTPARENT)" \
		CFLAGS="$(CFLAGS)" EFLAGS="$(EFLAGS)" LDFLAGS="$(LDFLAGS) -L.." \
		cjson_cflags="$(cjson_cflags)" cjson_libs="$(cjson_libs)" \
		json_cflags="$(jsonc_cflags)" jsonc_libs="$(jsonc_libs)" \
		jansson_cflags="$(jansson_cflags)" jansson_libs="$(jansson_libs)"

#cleaning
.PHONY: clean
clean:
	rm -f mustach libmustach*.so* *.o *.pc
	rm -f test-specs/*-test-specs test-specs/test-specs-*.last
	rm -rf *.gcno *.gcda coverage.info gcov-latest
	@$(MAKE) -C tests clean

# manpage
.PHONY: manuals
manuals: mustach.1.gz

mustach.1.gz: mustach.1.scd
	if which scdoc >/dev/null 2>&1; then scdoc < mustach.1.scd | gzip > mustach.1.gz; fi
