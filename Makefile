
mustach: mustach-tool.c  mustach.c  mustach.h  mustach-json-c.c  mustach-json-c.h
	$(CC) $(CFLAGS) -g -o mustach mustach-tool.c  mustach.c  mustach-json-c.c -ljson-c

.PHONY: test clean

test: mustach
	@make -C test1 test
	@make -C test2 test
	@make -C test3 test
	@make -C test4 test
	@make -C test5 test
	@make -C test6 test

clean:
	rm -f mustach
	@make -C test1 clean
	@make -C test2 clean
	@make -C test3 clean
	@make -C test4 clean
	@make -C test5 clean
	@make -C test6 clean


