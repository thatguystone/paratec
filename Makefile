TESTS = \
	exit_status \
	segfault \
	simple \
	timeout

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

simple: test/simple
	./$^ > /dev/null

timeout: test/timeout
	./$^ | grep "timed out" -q

segfault: test/segfault
	./$^ | grep "Segmentation fault" -q

exit_status: test/exit_status
	./$^ | grep "exit code=1" -q

test/%: test/%.c paratec.c paratec.h
	$(CC) $(CFLAGS) test/$*.c paratec.c -o test/$* $(LDFLAGS)
