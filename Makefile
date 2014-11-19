SHELL := /bin/bash

TESTS = \
	exit_status \
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

test: $(TESTS)

valgrind: test
	valgrind \
		--quiet \
		--tool=memcheck \
		--leak-check=full \
		--leak-resolution=high \
		--num-callers=20 \
		--track-origins=yes \
			./test

clean:
	rm -f $(TESTS:%=test/%)

exit_status: % : test/%
	./$^ | grep "exit code=1" -q

range: % : test/%
	./$^

segfault: % : test/%
	./$^ | grep "Segmentation fault" | grep after -q

simple: % : test/%
	./$^ > /dev/null

timeout: % : test/%
	./$^ | grep "timed out after" -q

updown: % : test/%
	./$^ | grep "up-down" -q

test/%: test/%.c paratec.c paratec.h
	$(CC) $(CFLAGS) test/$*.c paratec.c -o test/$* $(LDFLAGS)
