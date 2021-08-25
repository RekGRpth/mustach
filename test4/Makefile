.PHONY: test clean

test:
	@echo starting test
	@valgrind ../mustach json must > resu.last 2> vg.last
	@sed -i 's:^==[0-9]*== ::' vg.last
	@diff -w resu.ref resu.last && echo "result ok" || echo "ERROR! Result differs"
	@diff -wq vg.ref vg.last && echo "memory ok" || echo "memory differs"
	@echo

clean:
	rm -f resu.last vg.last

