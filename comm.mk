# This file is part of commake. Any changes made to this file should also be
# made to commake so that everyone can benefit.
#
# https://github.com/thatguystone/commake

#
# Hide annoying messages
#
MAKEFLAGS += --no-print-directory --no-builtin-rules

LLVM ?= llvm
ifeq ($(LLVM),llvm)
	CC = clang
	CXX = clang++
	export GCOV = llvm-cov
else
	override undefine GCOV
endif

#
# Useful variables
#
# ============================================================
#

PWD = $(CURDIR)
OS = $(shell uname -s)

SRC_DIR = src

FORMAT_DIRS += \
	$(SRC_DIR)/

ifeq ($(OS),Linux)
	export OS_LINUX = 1
endif

ifeq ($(OS),Darwin)
	export OS_OSX = 1
endif

ifndef NAME
$(error NAME must be set)
endif

SO = $(NAME).so
PC = $(NAME).pc
SONAME = $(SO).$(SOVERSION)
A = $(NAME).a
TEST_BIN = $(NAME).test
VMAP = $(NAME).v

#
# Install directories
#
# ============================================================
#

_PREFIX = /usr
_LIB_DIR = $(_PREFIX)/lib/$(DEB_HOST_MULTIARCH)
_BIN_DIR = $(_PREFIX)/bin
_PKGCFG_DIR = $(_LIB_DIR)/pkgconfig
_INCLUDE_DIR = $(_PREFIX)/include

PREFIX = $(abspath $(DESTDIR)$(_PREFIX))
LIB_DIR = $(abspath $(DESTDIR)$(_LIB_DIR))
PKGCFG_DIR = $(abspath $(DESTDIR)$(_PKGCFG_DIR))
INCLUDE_DIR = $(abspath $(DESTDIR)$(_INCLUDE_DIR))
BIN_DIR = $(abspath $(DESTDIR)$(_BIN_DIR))

#
# Commands
#
# ============================================================
#

define INST
	@echo INST $(1) $(2)
	@install -m 0644 $(1) $(2)
endef

define INST_BIN
	@echo INST_BIN $(1) $(2)
	@mkdir -p $(1)
	@install -m 0755 $(1) $(2)
endef

define INST_INTO
	@echo INST_INTO $(1) $(2)
	@mkdir -p $(1)
	@install -m 0644 -t $(1) $(2)
endef

define INST_RECUR
	echo INST_RECUR $(1) $(2); \
	for f in $(2); do \
		target="$(1)/$$(dirname $$f)"; \
		mkdir -p $$target; \
		install -m 0644 -t $$target $$f; \
	done
endef

define LN
	@echo LN $(1) $(2)
	@ln --relative -s $(1) $(2)
endef

define UNINST
	@echo UNINST $(1)
	@rm -rf $(1)
endef

CLANG_FORMAT = true
ifneq (,$(shell which clang-format))
	CLANG_FORMAT = clang-format
endif

GCOVR = true
ifneq (,$(shell which gcovr))
	GCOVR = gcovr
endif

VALGRIND_SUPP = valgrind.supp
ifneq (,$(wildcard VALGRIND_SUPP))
	_VALGRIND_ARGS += --suppressions=$(VALGRIND_SUPP)
endif

ifdef VALGRIND_GEN_SUPPS
	_VALGRIND_ARGS += --gen-suppressions=yes
endif

VALGRIND = \
	valgrind \
		--tool=memcheck \
		--leak-check=full \
		--leak-resolution=high \
		--show-leak-kinds=all \
		--num-callers=20 \
		--track-origins=yes \
		$(_VALGRIND_ARGS) \
			--

#
# Flags
#
# ============================================================
#

CFLAGS_ALL += \
	-g \
	-fPIC \
	-Wall \
	-Wcast-qual \
	-Wdisabled-optimization \
	-Werror \
	-Wextra \
	-Wformat=2 \
	-Wredundant-decls \
	-Wshadow \
	-Wundef \
	-Wwrite-strings \
	--param=ssp-buffer-size=4 \
	-D_FORTIFY_SOURCE=2 \
	-march=native \
	-mfpmath=sse \
	-msse \
	-msse2 \
	-MMD \
	-I$(SRC_DIR)/

# Travis' stuff is too outdated to use this flag. Awesome.
ifndef TRAVIS
	CFLAGS_ALL += -fstack-protector-strong
endif

# For both CFLAGS_TEST and CXXFLAGS_TEST
CFLAGS_TEST_BASE += \
	-O0 \
	--coverage \
	-fno-inline \
	-DTESTING

# Use for both CFLAGS and CFLAGS_TEST
CFLAGS_BASE += \
	$(CFLAGS_ALL) \
	-std=gnu11 \
	-Wdeclaration-after-statement \
	-Wmissing-prototypes \
	-Wstrict-prototypes

# Use for both CXXFLAGS and CXXFLAGS_TEST
CXXFLAGS_BASE += \
	$(CFLAGS_ALL) \
	-Wnon-virtual-dtor \
	-std=gnu++11

# For an optimized (release) build
CFLAGS = \
	$(CFLAGS_BASE) \
	-O3

# For building test binaries
CFLAGS_TEST += \
	$(CFLAGS_BASE) \
	$(CFLAGS_TEST_BASE)

# For an optimized (release) build
CXXFLAGS = \
	$(CXXFLAGS_BASE) \
	-O3

# For building test binaries
CXXFLAGS_TEST += \
	$(CXXFLAGS_BASE) \
	$(CFLAGS_TEST_BASE)

# For every build
LDFLAGS_BASE += \
	-rdynamic \
	-pthread

# For linking binaries
LDFLAGS += \
	$(LDFLAGS_BASE) \
	-pie

# For linking tests
LDFLAGS_TEST += \
	$(LDFLAGS) \
	--coverage

# For linking shared objects
LDFLAGS_SO += \
	$(LDFLAGS_BASE)

ifneq (,$(wildcard $(VMAP)))
	LDFLAGS_SO += \
		-Wl,--version-script=$(VMAP)
endif

ifdef OS_LINUX
	LDFLAGS_SO += \
		-Wl,-soname,$(SONAME)
endif

ifneq (,$(PKG_CFG_LIBS))
	CFLAGS_BASE += \
		$$(pkg-config --cflags $(PKG_CFG_LIBS))

	CXXFLAGS_BASE += \
		$$(pkg-config --cflags $(PKG_CFG_LIBS))

	LDFLAGS += \
		$$(pkg-config --libs $(PKG_CFG_LIBS))
endif

ifneq (,$(PKG_CFG_TEST_LIBS))
	CFLAGS_TEST += \
		$$(pkg-config --cflags $(PKG_CFG_TEST_LIBS))

	CXXFLAGS_TEST += \
		$$(pkg-config --cflags $(PKG_CFG_TEST_LIBS))

	LDFLAGS_TEST += \
		$$(pkg-config --libs $(PKG_CFG_TEST_LIBS))
endif

# The following flags only work with GNU's ld
ifneq (,$(findstring GNU ld,$(shell ld -v 2>&1)))
	LDFLAGS_BASE += \
		-fuse-ld=gold \
		-Wl,-z,now \
		-Wl,-z,relro
endif

#
# Typical object building stuff
#
# ============================================================
#

OBJECTS += \
	$(patsubst %.c,   %.o, $(wildcard $(SRC_DIR)/*.c)) \
	$(patsubst %.c,   %.o, $(wildcard $(SRC_DIR)/**/*.c)) \
	$(patsubst %.cc,  %.o, $(wildcard $(SRC_DIR)/*.cc)) \
	$(patsubst %.cc,  %.o, $(wildcard $(SRC_DIR)/**/*.cc)) \
	$(patsubst %.cpp, %.o, $(wildcard $(SRC_DIR)/*.cpp)) \
	$(patsubst %.cpp, %.o, $(wildcard $(SRC_DIR)/**/*.cpp))

BOBJECTS = $(filter-out %_test.o, $(OBJECTS))
TOBJECTS = $(patsubst %.o, %.to, $(OBJECTS))

DEP_INCS += \
	$(OBJECTS:.o=.d) \
	$(TOBJECTS:.to=.td)

#
# Misc rules
#
# ============================================================
#

all::

bench: $(TEST_BIN)
	PTBENCH=1 ./$(TEST_BIN)

test:: $(TEST_BIN)
	./$(TEST_BIN)
	@$(GCOVR) \
		--root=$(SRC_DIR)/ \
		--exclude=.*_test.* \
		--exclude=test/.* \
		$(GCOVR_ARGS)

test-valgrind: $(TEST_BIN)
	PTNOFORK=1 $(VALGRIND) ./$(TEST_BIN)

clean::
	@rm -f $(PC)
	@rm -f $(SONAME) $(A)
	@rm -f $(TEST_BIN)
	@rm -f $(OBJECTS)
	@rm -f $(OBJECTS:.o=.d)
	@rm -f $(OBJECTS:.o=.gcno)
	@rm -f $(OBJECTS:.o=.gcda)
	@rm -f $(TOBJECTS)
	@rm -f $(TOBJECTS:.to=.td)

_format:
	@$(CLANG_FORMAT) -i \
		$(foreach dir, $(FORMAT_DIRS), \
				$(wildcard $(dir)/**/*.c) \
				$(wildcard $(dir)/*.c) \
				$(wildcard $(dir)/**/*.c) \
				$(wildcard $(dir)/*.c) \
				$(wildcard $(dir)/**/*.cc) \
				$(wildcard $(dir)/*.cc) \
				$(wildcard $(dir)/**/*.cpp) \
				$(wildcard $(dir)/*.cpp) \
				$(wildcard $(dir)/**/*.h) \
				$(wildcard $(dir)/*.h) \
				$(wildcard $(dir)/**/*.hpp) \
				$(wildcard $(dir)/*.hpp))

#
# Rules to build all the files
#
# ============================================================
#

.PRECIOUS: %.o
.PRECIOUS: %.to

$(TEST_BIN): $(TOBJECTS)
	@echo '--- LD $@'
	@$(CXX) -o $@ $^ $(LDFLAGS_TEST)

$(A): $(BOBJECTS)
	@echo '--- AR $@'
	@$(AR) rcsT $@ $^ $(LIBAS)

$(SONAME): $(BOBJECTS)
	@echo '--- LD $@'
	@$(CXX) -shared $^ -o $@ $(LDFLAGS_SO)

%.o: %.c | _format
	@echo '--- CC $@'
	@$(CC) -c $(CFLAGS) -MF $*.d $< -o $@

%.o: %.cc | _format
%.o: %.cpp | _format
	@echo '--- CXX $@'
	@$(CXX) -c $(CXXFLAGS) -MF $*.d $< -o $@

%.to: %.c | _format
	@echo '--- CC $@'
	@rm -f $*.gcda $*.gcno
	@$(CC) -c $(CFLAGS_TEST) -MF $*.td $< -o $@

%.to: %.cc | _format
%.to: %.cpp | _format
	@echo '--- CXX $@'
	@rm -f $*.gcda $*.gcno
	@$(CXX) -c $(CXXFLAGS_TEST) -MF $*.td $< -o $@

ifeq (,$(findstring clean,$(MAKECMDGOALS)))
-include $(DEP_INCS)
endif
