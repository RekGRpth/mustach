.PHONY: test clean

P = ../..

CSRC =	test-custom-write.c \
	$P/mustach-json-c.c \
	$P/mustach-wrap.c \
	$P/mustach-helpers.c \
	$P/mustach.c \
	$P/mustach2.c \
	$P/mini-mustach.c

HSRC =	$P/mustach-json-c.h \
	$P/mustach.h \
	$P/mustach2.h \
	$P/mustach-helpers.h \
	$P/mini-mustach.h

test-custom-write: $(CSRC) $(HSRC)
	@echo building test-custom-write
	$(CC) $(CFLAGS) $(LDFLAGS) -I$P -g -o test-custom-write $(CSRC) -ljson-c

test: test-custom-write
	@mustach=./test-custom-write ../dotest.sh json -U must -l must -x must

clean:
	rm -f resu.last vg.last test-custom-write

