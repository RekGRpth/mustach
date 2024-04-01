#!/bin/sh

# Exit, with error message (hard failure)
exit_fail() {
    echo " FAIL: " "$@" >&2
    exit 1
}

mustach=${mustach:-../mustach}
echo starting test
if test "$NOVALGRIND" = 1
then
	$mustach "$@" > resu.last || exit_fail "ERROR! mustach command failed ($?)!"
else
	valgrind $mustach "$@" > resu.last 2> vg.last || exit_fail "ERROR! mustach command failed ($?)!"
	sed -i 's:^==[0-9]*== ::' vg.last
	awk '/^ *total heap usage: .* allocs, .* frees,.*/{if($$4-$$6)exit(1)}' vg.last || exit_fail "ERROR! Alloc/Free issue"
fi
if diff -w resu.ref resu.last
then
	echo "result ok"
else
	exit_fail "ERROR! Result differs"
fi
echo
exit 0
