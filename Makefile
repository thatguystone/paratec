NAME = libparatec
SOVERSION = 0

export override PTFILTER += ,-_,

all:: $(SONAME) $(A) $(PC)

test-all:
	$(MAKE) clean
	$(MAKE) LLVM= test
	$(MAKE) clean
	$(MAKE) LLVM=llvm test

install: all
	$(INST_INTO) $(INCLUDE_DIR) $(SRC_DIR)/paratec.h
	$(INST_INTO) $(LIB_DIR) $(A)
	$(INST_INTO) $(LIB_DIR) $(SONAME)
	$(INST_INTO) $(PKGCFG_DIR) $(PC)
	$(LN) $(LIB_DIR)/$(SONAME) $(LIB_DIR)/$(SO)

uninstall:
	$(UNINST) $(INCLUDE_DIR)/paratec.h
	$(UNINST) $(LIB_DIR)/$(SO)
	$(UNINST) $(LIB_DIR)/$(SONAME)
	$(UNINST) $(LIB_DIR)/$(A)
	$(UNINST) $(PKGCFG_DIR)/$(PC)

clean::
	@rm -f $(PC)
	@rm -f $(TEST_BINS)
	@rm -f $(TEST_BINS:=.o)
	@rm -f $(TEST_BINS:=.d)

include comm.mk

FORMAT_DIRS += \
	include/

CFLAGS_ALL += \
	-Iinclude/

#
# Extra build rules
#
# ==============================================================================
#

$(PC): libparatec.pc.in
	@echo '--- PC $@'
	@sed \
		-e 's|{PREFIX}|$(PREFIX)|' \
		$< > $@
