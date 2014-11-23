OS = $(shell uname -s)

TESTS = \
	abort \
	asserts \
	capture \
	cleanup \
	exit_status \
	filter \
	nocapture \
	nofork \
	nofork_fail \
	null_stdout \
	port \
	range \
	simple \
	timeout \
	updown

CFLAGS = \
	-g \
	-O2 \
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

ifeq ($(OS),Linux)
	LDFLAGS = \
		-lrt
endif

MEMCHECK = \
	valgrind \
		--quiet \
		--tool=memcheck \
		--leak-check=full \
		--leak-resolution=high \
		--num-callers=20 \
		--track-origins=yes

INSTALL = install -m 644
PREFIX ?= /usr
DEST = $(DESTDIR)/usr
INSTALL_INC_DIR = $(DEST)/include
INSTALL_LIB_DIR = $(DEST)/lib
INSTALL_PKGCFG_DIR = $(INSTALL_LIB_DIR)/pkgconfig

all: libparatec.so libparatec.a libparatec.pc

test: $(TESTS)
	@echo "SUCCESS"

valgrind: VG = $(MEMCHECK)
valgrind: test

install: all
	mkdir -p \
		$(INSTALL_INC_DIR) \
		$(INSTALL_LIB_DIR) \
		$(INSTALL_PKGCFG_DIR)
	$(INSTALL) paratec.h $(INSTALL_INC_DIR)
	$(INSTALL) libparatec.so libparatec.a $(INSTALL_LIB_DIR)
	$(INSTALL) libparatec.pc $(INSTALL_PKGCFG_DIR)

uninstall:
	rm -f $(INSTALL_INC_DIR)/paratec.h
	rm -f $(INSTALL_LIB_DIR)/libparatec.{so,a}
	rm -f $(INSTALL_PKGCFG_DIR)/libparatec.pc

clean:
	rm -f $(TESTS:%=test/%)
	rm -rf $(TESTS:%=test/%.dSYM)
	rm -f libparatec.so libparatec.a
	rm -f libparatec.pc

#
# Rules for building
#
# ==============================================================================
#

libparatec.so: paratec.c paratec.h
	$(CC) -O2 -shared -fPIC $(CFLAGS) $< -o $@

libparatec.a: paratec.c paratec.h
	$(CC) -c -O2 -fPIC $(CFLAGS) $< -o $@

libparatec.pc: libparatec.pc.in
	sed \
		-e 's|{PREFIX}|$(PREFIX)|' \
		$< > $@

test/%: test/%.c paratec.c paratec.h
	$(CC) $(CFLAGS) test/$*.c paratec.c -o test/$* $(LDFLAGS)

#
# Rules for test cases
#
# ==============================================================================
#

abort: % : test/%
	$(VG) ./$^ | grep "Aborted" | grep "after test start (test/abort.c" -q

asserts: % : test/%
	$(VG) ./$^ > /dev/null

capture: % : test/%
	$(VG) ./$^ -vv | [ `wc -l` -gt 1000 ]

cleanup: % : test/%
	$(VG) ./$^ | grep "cleanup_test, everybody clean up!" -q

exit_status: % : test/%
	$(VG) ./$^ | grep "exit code=1" -q

filter: % : test/%
	$(VG) ./$^ -f run --filter=another -f two,others | grep "4 tests"
	$(VG) ./$^ --filter=-fail -f -another -f another | grep "1 tests"
	$(VG) ./$^ --filter=-fail,-another -f another,two | grep "2 tests"

nocapture: % : test/%
	$(VG) ./$^ -n | grep "nocapture!" -q

nofork: % : test/%
	$(VG) ./$^ --nofork | grep "test/nofork.c:37 : Expected" -q
	$(VG) ./$^ --nofork | grep "test/nofork.c:43 : Expected" -q
	$(VG) ./$^ --nofork | grep "test start (test/nofork.c:18)" -q
	$(VG) ./$^ --nofork | grep "test/nofork.c:53 (test/nofork.c:18)" -q

nofork_fail: % : test/%
	$(VG) ./$^ --nofork || [ $$? -eq 134 ]

null_stdout: % : test/%
	$(VG) ./$^ -v | grep '\0' -q

port: % : test/%
	$(VG) ./$^ > /dev/null

range: % : test/%
	$(VG) ./$^ -v | grep "range_name:0:set_iter_name-" -q
	$(VG) ./$^ -v | grep "range:2" -q
	$(VG) ./$^ -v | grep ": not_range_with_set_name " -q

simple: % : test/%
	$(VG) ./$^ > /dev/null

timeout: % : test/%
	$(VG) ./$^ | grep "timed out" -q

updown: % : test/%
	$(VG) ./$^ -vv | grep "up-down" -q
