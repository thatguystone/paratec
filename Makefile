SHELL := /bin/bash

TESTS = \
	asserts \
	exit_status \
	filter \
	nofork \
	nofork_fail \
	null_stdout \
	range \
	segfault \
	simple \
	timeout \
	updown

CFLAGS = \
	-g \
	-Wall \
	-Wextra \
	-Wshadow \
	-Wformat=2 \
	-Werror \
	-Wdisabled-optimization \
	-fstack-protector \
	--param=ssp-buffer-size=4 \
	-D_FORTIFY_SOURCE=2 \
	-std=gnu99

LDFLAGS = \
	-lrt

MEMCHECK = \
	valgrind \
		--quiet \
		--tool=memcheck \
		--leak-check=full \
		--leak-resolution=high \
		--num-callers=20 \
		--track-origins=yes

test: $(TESTS)
	@echo "SUCCESS"

valgrind: VG = $(MEMCHECK)
valgrind: test

clean:
	rm -f $(TESTS:%=test/%)

asserts: % : test/%
	$(VG) ./$^ > /dev/null

exit_status: % : test/%
	$(VG) ./$^ | grep "exit code=1" -q

filter: % : test/%
	$(VG) ./$^ -f run --filter=another > /dev/null

nofork: % : test/%
	$(VG) ./$^ --nofork

nofork_fail: % : test/%
	$(VG) ./$^ --nofork || [ $$? -eq 139 ]

null_stdout: % : test/%
	$(VG) ./$^ -v | grep -P '\x00' -q

range: % : test/%
	$(VG) ./$^ > /dev/null

segfault: % : test/%
	$(VG) ./$^ | grep "Segmentation fault" | grep after -q

simple: % : test/%
	$(VG) ./$^ > /dev/null

timeout: % : test/%
	$(VG) ./$^ | grep "timed out" -q

updown: % : test/%
	$(VG) ./$^ -v | grep "up-down" -q

test/%: test/%.c paratec.c paratec.h
	$(CC) $(CFLAGS) test/$*.c paratec.c -o test/$* $(LDFLAGS)
