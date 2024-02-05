#!/bin/sh

mustach=${mustach:-../mustach}
echo starting test
if test "$NOVALGRIND" = 1
then
	$mustach "$@" > resu.last
else
	valgrind $mustach "$@" > resu.last 2> vg.last
	sed -i 's:^==[0-9]*== ::' vg.last
	awk '/^ *total heap usage: .* allocs, .* frees,.*/{if($$4-$$6)exit(1)}' vg.last || echo "ERROR! Alloc/Free issue"
fi
if diff -w resu.ref resu.last
then
	echo "result ok"
else
	echo "ERROR! Result differs"
fi
echo

