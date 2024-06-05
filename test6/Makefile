.PHONY: test clean

test-custom-write: test-custom-write.c ../mustach-json-c.h ../mustach-json-c.c ../mustach-wrap.c ../mustach.h ../mustach2.h ../mini-mustach.h ../mustach.c ../mustach2.c ../mini-mustach.c
	@echo building test-custom-write
	$(CC) $(CFLAGS) $(LDFLAGS) -g -o test-custom-write test-custom-write.c  ../mustach.c ../mustach2.c ../mini-mustach.c ../mustach-helpers.c    ../mustach-json-c.c ../mustach-wrap.c -ljson-c

test: test-custom-write
	@mustach=./test-custom-write ../dotest.sh json -U must -l must -x must

clean:
	rm -f resu.last vg.last test-custom-write

