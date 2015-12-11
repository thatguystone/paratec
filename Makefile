SOVERSION = 0
NAME = libparatec
A = $(NAME).a
O = $(NAME).o
SO = $(NAME).so
SONAME = $(SO).$(SOVERSION)
PC = libparatec.pc

OS = $(shell uname -s)

TESTS = \
	abort \
	asserts \
	bench \
	capture \
	cleanup \
	errno \
	exit_status \
	filter \
	nocapture \
	nofork \
	nofork_fail \
	null_stdout \
	paratecv \
	port \
	range \
	simple \
	skip \
	timeout \
	updown \
	wait_for

ifneq (,$(findstring clang++,$(CXX)))
	TESTS := $(TESTS) \
		cpp
endif

CFLAGS_BASE = \
	-g \
	-O2 \
	-Wall \
	-Wextra \
	-Wcast-qual \
	-Wdisabled-optimization \
	-Wformat=2 \
	-Wredundant-decls \
	-Wshadow \
	-Wundef \
	-Wwrite-strings \
	-Werror \
	-fstack-protector \
	--param=ssp-buffer-size=4 \
	-D_FORTIFY_SOURCE=2 \
	-mfpmath=sse \
	-msse \
	-msse2

CFLAGS = \
	$(CFLAGS_BASE) \
	-Wdeclaration-after-statement \
	-Wmissing-prototypes \
	-Wstrict-prototypes \
	-std=gnu99

CXXFLAGS = \
	$(CFLAGS_BASE) \
	-std=gnu++0x

ifeq ($(OS),Linux)
	LDFLAGS += \
		-Wl,-soname,$(SONAME) \
		-lrt
endif

# The following flags only work with GNU's ld
ifneq (,$(findstring GNU ld,$(shell ld -v 2>&1)))
	LDFLAGS += \
		-Wl,-z,now \
		-Wl,-z,relro
endif

MEMCHECK = \
	valgrind \
		--quiet \
		--tool=memcheck \
		--leak-check=full \
		--leak-resolution=high \
		--num-callers=20 \
		--track-origins=yes

INSTALL = install -D -m 644
PREFIX ?= /usr
DEST = $(DESTDIR)/usr
INSTALL_INC_DIR = $(DEST)/include
INSTALL_LIB_DIR = $(DEST)/lib/$(DEB_HOST_MULTIARCH)
INSTALL_PKGCFG_DIR = $(INSTALL_LIB_DIR)/pkgconfig

all: $(SO) $(A) $(PC)

test: $(TESTS)
	@echo "SUCCESS"

test-all:
	$(MAKE) clean
	CC=gcc CXX=g++ $(MAKE) test
	$(MAKE) clean
	CC=clang CXX=clang++ $(MAKE) test

valgrind: VG = $(MEMCHECK)
valgrind: test

format:
	[ ! -f "`which clang-format`" ] && true || \
		clang-format -i \
			*.c *.h \
			test/*.c test/*.cpp

install: all
	# Can remove this mkdir once trusty support is no longer necessary
	mkdir -p \
		$(INSTALL_INC_DIR) \
		$(INSTALL_LIB_DIR) \
		$(INSTALL_PKGCFG_DIR)
	$(INSTALL) paratec.h -t $(INSTALL_INC_DIR)
	$(INSTALL) $(A) -t $(INSTALL_LIB_DIR)
	$(INSTALL) $(SO) $(INSTALL_LIB_DIR)/$(SONAME)
	$(INSTALL) $(PC) -t $(INSTALL_PKGCFG_DIR)
	ln -s -r $(INSTALL_LIB_DIR)/$(SONAME) $(INSTALL_LIB_DIR)/$(SO)

uninstall:
	rm -f $(INSTALL_INC_DIR)/paratec.h
	rm -f $(INSTALL_LIB_DIR)/$(SO)
	rm -f $(INSTALL_LIB_DIR)/$(SONAME)
	rm -f $(INSTALL_LIB_DIR)/$(A)
	rm -f $(INSTALL_PKGCFG_DIR)/$(PC)

clean:
	rm -f $(TESTS:%=test/%) test/cpp
	rm -rf $(TESTS:%=test/%.dSYM)
	rm -f test/paratec.o
	rm -f $(SO) $(A) $(O) $(PC)

#
# Rules for building
#
# ==============================================================================
#

$(SO): paratec.c paratec.h | format
	$(CC) -O2 -shared -fPIC $(CFLAGS) $< -o $@ $(LDFLAGS)

$(A): $(O)
	ar -r $@ $^ > /dev/null 2>&1

$(O): paratec.c paratec.h | format
	$(CC) -c -O2 -fPIC $(CFLAGS) $< -o $@

$(PC): libparatec.pc.in
	sed \
		-e 's|{PREFIX}|$(PREFIX)|' \
		$< > $@

test/paratec.o: paratec.c paratec.h
	$(CC) $(CFLAGS) -c -o $@ $<

test/%: test/%.c test/paratec.o
	$(CC) $(CFLAGS) test/$*.c paratec.c -o test/$* $(LDFLAGS)

#
# Rules for test cases
#
# ==============================================================================
#

$(TESTS): |format

asserts paratecv port simple wait_for: % : test/%
	$(VG) ./$^ > /dev/null

abort: % : test/%
	$(VG) ./$^ | grep "Abort" | grep "after test/abort.c:14 (last test assert: test start)" -q

bench: % : test/%
	OUT=`$(VG) ./$^ -b`; \
		echo "$$OUT" | grep "no bench for you" -q && \
		echo "$$OUT" | grep "BENCH" -q

capture: % : test/%
	$(VG) ./$^ -vvv | [ `wc -l` -gt 1000 ]

cleanup: % : test/%
	$(VG) ./$^ | grep "cleanup_test, everybody clean up!" -q
	$(VG) ./$^ -j 2 | grep "i 23120 cleanup" -q
	$(VG) ./$^ -j 2 | grep "i 23121 cleanup" -q

cpp: test/cpp.cpp test/paratec.o
	$(CXX) $(CXXFLAGS) $^ -o test/$@ $(LDFLAGS)
	$(VG) ./test/$@ | grep "cleanup_test, everybody clean up!" -q

errno: % : test/%
	$(VG) ./$^ | grep "Expected -1 == 0. Error 98:" -q

exit_status: % : test/%
	$(VG) ./$^ | grep "expected exit code=0, got=1" -q

filter: % : test/%
	$(VG) ./$^ -f run --filter=another -f two,others | grep "4 tests"
	$(VG) ./$^ --filter=-fail -f -another -f another | grep "1 tests"
	$(VG) ./$^ --filter=-fail,-another -f another,two | grep "2 tests"

nocapture: % : test/%
	$(VG) ./$^ -n | grep "nocapture!" -q

nofork: % : test/%
	$(VG) ./$^ --nofork | grep "test/nofork.c:37 : Expected" -q
	$(VG) ./$^ --nofork | grep "test/nofork.c:43 : Expected" -q
	$(VG) ./$^ --nofork | grep "test/nofork.c:18 (last test assert: test start)" -q
	$(VG) ./$^ --nofork | grep "test/nofork.c:18 (last test assert: test/nofork.c:53)" -q

nofork_fail: % : test/%
	$(VG) ./$^ --nofork || [ $$? -eq 134 ]

null_stdout: % : test/%
	$(VG) ./$^ -v | grep '\0' -q

range: % : test/%
	$(VG) ./$^ -v | grep "range_name:0:set_iter_name-" -q
	$(VG) ./$^ -v | grep "range:2" -q
	$(VG) ./$^ -v | grep ": not_range_with_set_name " -q

skip: % : test/%
	$(VG) ./$^ -vvv | grep "1 skipped\|skipping this test" | [ `wc -l` -eq 2 ]
	$(VG) ./$^ -s | grep "1 skipped\|skipping this test" | [ `wc -l` -eq 2 ]

timeout: % : test/%
	$(VG) ./$^ | grep "after test start : timed out" -q
	$(VG) ./$^ | grep "after test/timeout.c:21 : timed out" -q

updown: % : test/%
	$(VG) ./$^ -vvv | grep "up-down" -q
