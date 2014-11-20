OS = $(shell uname -s)

TESTS = \
	asserts \
	cleanup \
	exit_status \
	filter \
	nofork \
	nofork_fail \
	null_stdout \
	port \
	range \
	segfault \
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

asserts: % : test/%
	$(VG) ./$^ > /dev/null

cleanup: % : test/%
	$(VG) ./$^ | grep "cleanup_test, everybody clean up!" -q

exit_status: % : test/%
	$(VG) ./$^ | grep "exit code=1" -q

filter: % : test/%
	$(VG) ./$^ -f run --filter=another -f two,others | grep "4 tests"

nofork: % : test/%
	$(VG) ./$^ --nofork

nofork_fail: % : test/%
	$(VG) ./$^ --nofork || [ $$? -eq 139 ]

null_stdout: % : test/%
	$(VG) ./$^ -v | grep '\0' -q

port: % : test/%
	$(VG) ./$^ > /dev/null

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
