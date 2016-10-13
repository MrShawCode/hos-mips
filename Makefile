#--------------------------------------------------------------
# Just run 'make'. If you want to change the config, please
# alter files in config directory.
#--------------------------------------------------------------

TOPDIR=$(shell pwd)
Q :=@

KTREE = $(TOPDIR)/src/kern-ucore
ifndef O
OBJPATH_ROOT := $(TOPDIR)/obj
else
OBJPATH_ROOT := $(abspath $(O))
endif
export TOPDIR KTREE OBJPATH_ROOT

KCONFIG_AUTOCONFIG=$(TOPDIR)/Makefile.config
#User Path
USER_OBJ_ROOT := $(OBJPATH_ROOT)/user-ucore
BIN := $(USER_OBJ_ROOT)/bin
USER_APPLIST:= pwd cat sh ls cp echo guessnum #link mkdir rename unlink lsmod insmod rmmod mount umount
USER_APP_BINS:= $(addprefix $(BIN)/, $(USER_APPLIST))

CONFIG_SHELL:=$(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	else if [ -x /bin/bash ]; then echo /bin/bash; \
	else echo sh; fi; fi)

MAKEFLAGS += -rR --no-print-directory

-include $(KCONFIG_AUTOCONFIG)

#### CROSS COMPILE HERE ####
ARCH ?= $(patsubst "%",%,$(UCONFIG_ARCH))
BOARD ?= default
CROSS_COMPILE ?= $(UCONFIG_CROSS_COMPILE)

export CONFIG_SHELL quiet Q KBUILD_VERBOSE
export ARCH CROSS_COMPILE
export KCONFIG_AUTOCONFIG


TARGET_CC := $(CROSS_COMPILE)gcc
TARGET_LD := $(CROSS_COMPILE)ld
TARGET_AR := $(CROSS_COMPILE)ar
TARGET_STRIP := $(CROSS_COMPILE)strip
TARGET_OBJCOPY := $(CROSS_COMPILE)objcopy

export TARGET_CC TARGET_LD TARGET_AR TARGET_LD TARGET_STRIP TARGET_OBJCOPY

#FPGA or not

ifeq  ($(ON_FPGA), y)
MACH_DEF := -DMACH_FPGA
else
MACH_DEF := -DMACH_QEMU
endif

ifndef HOSTAR
HOSTAR:=ar
endif
ifndef HOSTAS
HOSTAS:=as
endif
ifndef HOSTCC
HOSTCC:=gcc
else
endif
ifndef HOSTCXX
HOSTCXX:=g++
endif
ifndef HOSTLD
HOSTLD:=ld
endif
ifndef HOSTLN
HOSTLN:=ln
endif
HOSTAR:=$(shell $(CONFIG_SHELL) -c "which $(HOSTAR)" || type -p $(HOSTAR) || echo ar)
HOSTAS:=$(shell $(CONFIG_SHELL) -c "which $(HOSTAS)" || type -p $(HOSTAS) || echo as)
HOSTCC:=$(shell $(CONFIG_SHELL) -c "which $(HOSTCC)" || type -p $(HOSTCC) || echo gcc)
HOSTCXX:=$(shell $(CONFIG_SHELL) -c "which $(HOSTCXX)" || type -p $(HOSTCXX) || echo g++)
HOSTLD:=$(shell $(CONFIG_SHELL) -c "which $(HOSTLD)" || type -p $(HOSTLD) || echo ld)
HOSTLN:=$(shell $(CONFIG_SHELL) -c "which $(HOSTLN)" || type -p $(HOSTLN) || echo ln)
ifndef CFLAGS_FOR_BUILD
CFLAGS_FOR_BUILD:=-g -O2
endif
export HOSTAR HOSTAS HOSTCC HOSTCXX HOSTLD

HOSTCFLAGS=$(CFLAGS_FOR_BUILD)
export HOSTCFLAGS

PHONY+= clean all sfsimg

PHONY+= $(OBJPATH_ROOT) $(KCONFIG_AUTOCONFIG)
all: sfsimg kernel

PHONY += kernel userlib userapp

kernel: $(OBJPATH_ROOT) $(KCONFIG_AUTOCONFIG) $(SFSIMG_LINK)
	$(Q)$(MAKE)  -C $(KTREE) -f $(KTREE)/Makefile.build

userlib: $(OBJPATH_ROOT) $(KCONFIG_AUTOCONFIG)
	$(Q)$(MAKE) -f $(TOPDIR)/user/Makefile -C $(TOPDIR)/user  all

define re-user-app
$1: $(BUILD_DIR) $(addsuffix .o,$1) $(USER_LIB)
	@echo LINK $$@
	sed 's/$$$$FILE/$(notdir $1)/g' user/user-ucore/tools/piggy.S.in > $(BIN)/piggy.S
	$(CROSS_COMPILE)as $(BIN)/piggy.S -o $$@.piggy.o
	echo "-------resize bin---------"
endef

$(foreach bdir,$(USER_APP_BINS),$(eval $(call re-user-app,$(bdir))))

userapp: $(OBJPATH_ROOT) $(KCONFIG_AUTOCONFIG)
	$(Q)$(MAKE) -f $(TOPDIR)/user/user-ucore/Makefile -C $(TOPDIR)/user/user-ucore  all

## TOOLS 

ifdef UCONFIG_HAVE_SFS
TOOLS_MKSFS_DIR := $(TOPDIR)/tool
TOOLS_MKSFS := $(OBJPATH_ROOT)/mksfs
$(TOOLS_MKSFS): | $(OBJPATH_ROOT)
	$(Q)$(MAKE) CC=$(HOSTCC) -f $(TOOLS_MKSFS_DIR)/Makefile -C $(TOOLS_MKSFS_DIR) all

## image
SFSIMG_LINK := $(OBJPATH_ROOT)/sfs.img
SFSIMG_FILE := $(OBJPATH_ROOT)/sfs-orig.img
TMPSFS := $(OBJPATH_ROOT)/.tmpsfs
sfsimg: $(SFSIMG_LINK)

$(SFSIMG_LINK): $(SFSIMG_FILE)
	@ln -sf sfs-orig.img $@


$(SFSIMG_FILE): $(TOOLS_MKSFS) userlib userapp FORCE | $(OBJPATH_ROOT)
	@echo Making $@
	@mkdir -p $(TMPSFS)
	@mkdir -p $(TMPSFS)/lib/modules
ifeq  ($(ON_FPGA), y)
#here, you should add your own app to the $(TMPSFS)
#
#	example here:
#	@make -C src/apps/snake
#	@cp src/apps/snake/snake $(TMPSFS)
#
else
#do nothing
endif

	@cp -r $(OBJPATH_ROOT)/user/bin $(TMPSFS)
ifneq ($(UCORE_TEST),)
	@cp -r $(OBJPATH_ROOT)/user/testbin $(TMPSFS)
endif
	@$(Q)$(MAKE) -f $(TOPDIR)/user/user-ucore/Makefile -C $(TOPDIR)/user/user-ucore initial_dir
	@if [ $(ARCH) = "mips" ]; \
	then \
		echo " mips"; \
		cp -r $(TOPDIR)/user/user-ucore/_initial/hello.txt $(TMPSFS); \
		rm -f $@; \
		dd if=/dev/zero of=$@ count=2400; \
	else \
		echo -n $(ARCH)." not mips"; \
		cp -r $(TOPDIR)/user/user-ucore/_initial/* $(TMPSFS); \
		rm -f $@; \
		dd if=/dev/zero of=$@ bs=256K count=$(UCONFIG_SFS_IMAGE_SIZE); \
	fi
	@$(TOOLS_MKSFS) $@ $(TMPSFS)
	@rm -rf $(TMPSFS)

endif

$(OBJPATH_ROOT):
	-mkdir -p $@

$(CONFIG_DIR):
	-mkdir -p $@

clean:
	@echo CLEAN ALL
	$(Q)rm -f cscope.*
	$(Q)rm -f $(SFSIMG_FILE)
	$(Q)rm -rf $(OBJPATH_ROOT)
	$(Q)$(MAKE) -C $(KTREE) -f Makefile.build clean
	$(Q)$(MAKE) -f $(TOPDIR)/user/Makefile -C $(TOPDIR)/user  clean
	$(Q)$(MAKE) -f $(TOPDIR)/user/user-ucore/Makefile -C $(TOPDIR)/user/user-ucore  clean

indent:
	$(Q)find $(TOPDIR)/src -name *.c -or -name *.h | grep -vf $(TOPDIR)/misc/indent-whitelist | xargs $(TOPDIR)/misc/Lindent
	$(Q)rm -rf `find $(TOPDIR)/src -name *~`

cscope:
	find ./ -name "*.c" > cscope.files
	find ./ -name "*.h" >> cscope.files
	find ./ -name "*.S" >> cscope.files
	cscope -bqk

FORCE:

PHONY += FORCE

.PHONY: $(PHONY)
