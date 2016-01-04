NAME = libparatec
SOVERSION = 1

override PTFILTER += ,-_,
export PTFILTER

include comm.mk

all:: $(SONAME) $(A) $(PC)

test-all:
	$(MAKE) clean
	$(MAKE) LLVM= test
	$(MAKE) clean
	$(MAKE) LLVM=llvm test

install: all
	$(call INST_INTO, $(INCLUDE_DIR), $(SRC_DIR)/paratec.h)
	$(call INST_INTO, $(LIB_DIR), $(A))
	$(call INST_INTO, $(LIB_DIR), $(SONAME))
	$(call INST_INTO, $(PKGCFG_DIR), $(PC))
	$(call LN, $(LIB_DIR)/$(SONAME), $(LIB_DIR)/$(SO))

uninstall:
	$(call UNINST, $(INCLUDE_DIR)/paratec.h)
	$(call UNINST, $(LIB_DIR)/$(SO))
	$(call UNINST, $(LIB_DIR)/$(SONAME))
	$(call UNINST, $(LIB_DIR)/$(A))
	$(call UNINST, $(PKGCFG_DIR)/$(PC))

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
