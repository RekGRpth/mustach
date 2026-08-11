#!/bin/sh

# Exit, with error message (hard failure)
exit_fail() {
    echo " FAIL: " "$@" >&2
    exit 1
}

# check valgrind
if ! valgrind --version > /dev/null 2>&1
then
	[ "$VALGRIND" = 1 ] && exit_fail "no valgrind"
	NOVALGRIND=1
fi

# check mustach
mustach="${mustach:-../../mustach}"
backend=$($mustach --backend) || exit_fail "ERROR! Can't get mustach backend!"
echo "starting test"
if [ "$NOVALGRIND" = 1 ]
then
	$mustach "$@" > resu.last || exit_fail "ERROR! mustach command failed ($?)!"
else
	valgrind $mustach "$@" > resu.last 2> vg.last || exit_fail "ERROR! valgrind + mustach command failed ($?)!"
	sed -i 's:^==[0-9]*== ::' vg.last
	awk '/^ *total heap usage: .* allocs, .* frees,.*/{if($$4-$$6)exit(1)}' vg.last || exit_fail "ERROR! Alloc/Free issue"
fi

# check the ref
test -f resu.ref.$backend && ref=resu.ref.$backend || ref=resu.ref
if diff -w $ref resu.last
then
	echo "result ok"
else
	exit_fail "ERROR! Result differs"
fi
echo
exit 0
