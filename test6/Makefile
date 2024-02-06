.PHONY: test clean

test-custom-write: test-custom-write.c ../mustach-json-c.h ../mustach-json-c.c ../mustach-wrap.c ../mustach.h ../mustach.c
	@echo building test-custom-write
	$(CC) $(CFLAGS) $(LDFLAGS) -g -o test-custom-write test-custom-write.c  ../mustach.c  ../mustach-json-c.c ../mustach-wrap.c -ljson-c

test: test-custom-write
	@mustach=./test-custom-write ../dotest.sh json -U must -l must -x must

clean:
	rm -f resu.last vg.last test-custom-write

