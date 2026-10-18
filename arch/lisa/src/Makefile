############################################################################
# arch/lisa/src/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(TOPDIR)/tools/Make.defs
include chip/Make.defs

# Tools
# CFLAGS, CPPFLAGS, ASFLAGS, LDFLAGS are set in $(TOPDIR)/Make.defs

ARCH_SRCDIR = $(TOPDIR)$(DELIM)arch$(DELIM)$(CONFIG_ARCH)$(DELIM)src
CFLAGS += ${INCDIR_PREFIX}$(ARCH_SRCDIR)$(DELIM)
CFLAGS += ${INCDIR_PREFIX}$(ARCH_SRCDIR)$(DELIM)chip
CFLAGS += ${INCDIR_PREFIX}$(ARCH_SRCDIR)$(DELIM)common
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)sched
CPPFLAGS += -D__ASSEMBLY__

# Files and directories

HEAD_OBJ = $(HEAD_ASRC:$(ASMEXT)=$(OBJEXT))
STARTUP_OBJS ?= $(HEAD_OBJ)

ASRCS = $(CHIP_ASRCS) $(CMN_ASRCS)
AOBJS = $(ASRCS:$(ASMEXT)=$(OBJEXT))

CSRCS = $(CHIP_CSRCS) $(CMN_CSRCS)
COBJS = $(CSRCS:.c=$(OBJEXT))

SRCS = $(ASRCS) $(CSRCS)
OBJS = $(AOBJS) $(COBJS)

DEPSRCS = $(CSRCS)

BOARDDIR = $(TOPDIR)/arch/$(CONFIG_ARCH)/src/board

VPATH = chip:common:board

# The code banks the bank map uses: the window of bank n is linked at byte
# n * 0x8000 of the image (word 0x4000 of the bank)

BANKS = $(shell awk '!/^[ \t]*(\#|$$)/ && $$2 > 0 { print $$2 }' $(LISA_BANK_MAP) | sort -un)

# Targets

all: $(HEAD_OBJ) libarch$(LIBEXT)

.PHONY: board/libboard$(LIBEXT) nuttx.lnk

$(AOBJS) $(HEAD_OBJ): %$(OBJEXT): %$(ASMEXT) asm_mem.h
	$(call ASSEMBLE, $<, $@)

$(COBJS): %$(OBJEXT): %.c
	$(call COMPILE, $<, $@)

# The RAM layout for the head: the idle thread's stack at the top of RAM
# (SP is 15 bits: the stack starts at the last byte), the heap below it

asm_mem.h:
	@echo "	CONFIG_STACK_END == ($(CONFIG_RAM_START) + $(CONFIG_RAM_SIZE) - 1)" > asm_mem.h
	@echo "	CONFIG_STACK_BASE == ($(CONFIG_RAM_START) + $(CONFIG_RAM_SIZE) - $(CONFIG_IDLETHREAD_STACKSIZE))" >> asm_mem.h

# Combine all objects in this directory into a library

libarch$(LIBEXT): asm_mem.h $(OBJS)
	$(call ARCHIVE, $@, $(OBJS))

# This builds the libboard library in the board/ subdirectory

board/libboard$(LIBEXT):
	$(Q) $(MAKE) -C board libboard$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

# The link script: the head first (its area order is the layout and its
# vectors are at code address 0), then the libraries - NuttX's, the
# board's and sdcc's lisa-rev2 (the support routines and the banked-call
# trampoline) - and the code banks

nuttx.lnk:
	@echo "LD: nuttx.lnk"
	@echo "-muwx" > nuttx.lnk
	@echo "-i nuttx.ihx" >> nuttx.lnk
ifeq ($(CONFIG_DEBUG_SYMBOLS),y)
	@echo "-y" >> nuttx.lnk
endif
	@echo "-b HOME=0x0000" >> nuttx.lnk
	@echo "-b DATA=0x0000" >> nuttx.lnk
	$(Q) for b in $(BANKS); do printf -- "-b BANK%d=0x%X\n" $$b $$(($$b * 32768)) >> nuttx.lnk; done
	@echo "-k $(SDCC_LIBDIR)/" >> nuttx.lnk
	@echo "-l $(BOARDDIR)/libboard$(LIBEXT)" >> nuttx.lnk
	$(Q) for LIB in $(LINKLIBS); do echo "-l $(TOPDIR)/staging/$$LIB" >> nuttx.lnk; done
	@echo "-l lisa" >> nuttx.lnk
	@echo "$(HEAD_OBJ)" >> nuttx.lnk
	@echo "" >> nuttx.lnk
	@echo "-e" >> nuttx.lnk

nuttx$(EXEEXT): asm_mem.h $(HEAD_OBJ) board/libboard$(LIBEXT) nuttx.lnk
	@echo "LD: nuttx.ihx"
	$(Q) $(LD) -nf nuttx.lnk
	$(Q) cp -f nuttx.ihx $(TOPDIR)/nuttx$(EXEEXT)
	$(Q) cp -f nuttx.map $(TOPDIR)/nuttx.map
	$(Q) if [ -f nuttx.cdb ]; then cp -f nuttx.cdb $(TOPDIR)/nuttx.cdb; fi

# This is part of the top-level export target

export_startup: $(STARTUP_OBJS)
	$(Q) if [ -d "$(EXPORT_DIR)/startup" ]; then \
		cp -f $(STARTUP_OBJS) "$(EXPORT_DIR)/startup"; \
	 else \
		echo "$(EXPORT_DIR)/startup does not exist"; \
		exit 1; \
	 fi

# Build dependencies

.depend: Makefile asm_mem.h chip/Make.defs $(DEPSRCS) $(TOPDIR)$(DELIM).config
	$(Q) if [ -e board/Makefile ]; then \
		$(MAKE) -C board depend ; \
	fi
	$(Q) $(MKDEP) --obj-suffix $(OBJEXT) --dep-path chip --dep-path common "$(CC)" -- $(CFLAGS) -- $(DEPSRCS) >Make.dep
	$(Q) touch $@

depend: .depend

context::

clean:
	$(Q) if [ -e board/Makefile ]; then \
		$(MAKE) -C board clean ; \
	fi
	$(call DELFILE, asm_mem.h)
	$(call DELFILE, nuttx.*)
	$(call DELFILE, libarch$(LIBEXT))
	$(call CLEAN)

distclean:: clean
	$(Q) if [ -e board/Makefile ]; then \
		$(MAKE) -C board distclean ; \
	fi
	$(call DELFILE, Make.dep)
	$(call DELFILE, .depend)

-include Make.dep
