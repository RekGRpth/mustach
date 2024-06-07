P = ..


# testing
ifeq ($(valgrind),no)
 NOVALGRIND := 1
else ifeq ($(valgrind),yes)
 VALGRIND := 1
endif
export NOVALGRIND
export VALGRIND

.PHONY: test test-basic test-specs
test: basic-tests spec-tests

basic-tests: $P/mustach
	@$(MAKE) -C test1 test
	@$(MAKE) -C test2 test
	@$(MAKE) -C test3 test
	@$(MAKE) -C test4 test
	@$(MAKE) -C test5 test
	@$(MAKE) -C test6 test
	@$(MAKE) -C test7 test
	@$(MAKE) -C test8 test
	@test "$(TESTPARENT)" -eq 0 || $(MAKE) -C test9 test

spec-tests: $(TESTSPECS)

test-specs/test-specs-%: test-specs/%-test-specs test-specs/specs
	./$< test-specs/spec/specs/[a-z]*.json > $@.last || true
	test "$(TESTPARENT)" -eq 0 || ./$< test-specs/spec/specs/~inheritance.json >> $@.last || true
	diff $@.ref $@.last

COREOBJS = $P/mini-mustach.o \
	   $P/mustach2.o \
	   $P/mustach-helpers.o \
	   $P/mustach-wrap.o \
	   $P/mustach.o

test-specs/cjson-test-specs.o: test-specs/test-specs.c $P/mustach.h $P/mustach-wrap.h $P/mustach-cjson.h
	$(CC) -I.. -c $(EFLAGS) $(CFLAGS) $(cjson_cflags) -DTEST=TEST_CJSON -o $@ $<

test-specs/cjson-test-specs: test-specs/cjson-test-specs.o $P/mustach-cjson.o $(COREOBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(cjson_libs)

test-specs/json-c-test-specs.o: test-specs/test-specs.c $P/mustach.h $P/mustach-wrap.h $P/mustach-json-c.h
	$(CC) -I.. -c $(EFLAGS) $(CFLAGS) $(jsonc_cflags) -DTEST=TEST_JSON_C -o $@ $<

test-specs/json-c-test-specs: test-specs/json-c-test-specs.o $P/mustach-json-c.o $(COREOBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(jsonc_libs)

test-specs/jansson-test-specs.o: test-specs/test-specs.c $P/mustach.h $P/mustach-wrap.h $P/mustach-jansson.h
	$(CC) -I.. -c $(EFLAGS) $(CFLAGS) $(jansson_cflags) -DTEST=TEST_JANSSON -o $@ $<

test-specs/jansson-test-specs: test-specs/jansson-test-specs.o $P/mustach-jansson.o $(COREOBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(jansson_libs)

.PHONY: test-specs/specs
test-specs/specs:
	if ! test -d test-specs/spec; then \
		git -C test-specs clone https://github.com/mustache/spec.git; \
	fi
	git -C test-specs/spec checkout $(VSPEC) -B test;

#cleaning
.PHONY: clean
clean:
	@$(MAKE) -C test1 clean
	@$(MAKE) -C test2 clean
	@$(MAKE) -C test3 clean
	@$(MAKE) -C test4 clean
	@$(MAKE) -C test5 clean
	@$(MAKE) -C test6 clean
	@$(MAKE) -C test7 clean
	@$(MAKE) -C test8 clean
	@$(MAKE) -C test9 clean

