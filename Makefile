# Build environments
TOPDIR := $(shell pwd)
OBJPATH_ROOT := $(TOPDIR)/obj
export TOPDIR OBJPATH_ROOT

include Makefile.config

# Compilers
TARGET_CC := $(CROSS_COMPILER)gcc
TARGET_LD := $(CROSS_COMPILER)ld
TARGET_AR := $(CROSS_COMPILER)ar
TARGET_STRIP := $(CROSS_COMPILER)strip
TARGET_OBJCOPY := $(CROSS_COMPILER)objcopy
export TARGET_CC TARGET_LD TARGET_AR TARGET_LD TARGET_STRIP TARGET_OBJCOPY 

HOST_CC := $(HOST_COMPILER)gcc
HOST_LD := $(HOST_COMPILER)ld
HOST_AR := $(HOST_COMPILER)ar
HOST_STRIP := $(HOST_COMPILER)strip
HOST_OBJCOPY := $(HOST_COMPILER)objcopy
export HOST_CC HOST_LD HOST_AR HOST_LD HOST_STRIP HOST_OBJCOPY 

CFLAGS += -Wall -Wno-format -Wno-unused -Werror -gstabs -I include -I lib
export CFLAGS

# Targets 
TOOLS_MKSFS := $(OBJPATH_ROOT)/mksfs
DEF_USER_APPS := pwd cat sh ls cp echo guessnum 
USER_APPLIST := $(DEF_USER_APPS) $(EXTRA_USER_APPS)

PHONY = clean all kernel

# Composed
#all: $(TOOLS_MKSFS) kernel
all: kernel
	
clean:
	@echo Will clean all obj files.
	@rm -fr $(OBJPATH_ROOT)
	@rm -fr cscope.*

cscope:
	find ./ -name "*.c" > cscope.files
	find ./ -name "*.h" >> cscope.files
	find ./ -name "*.S" >> cscope.files
	cscope -bqk

$(OBJPATH_ROOT):
	-mkdir -p $@

# Tools
$(TOOLS_MKSFS): | $(OBJPATH_ROOT)
	$(MAKE) CC=$(HOST_CC) -f $(TOPDIR)/tools/Makefile.frag -C $(TOPDIR)/tools all

# Kernel
kernel: $(OBJPATH_ROOT)
	@echo Going to build kernel.
	$(MAKE) CC=$(TARGET_CC) -f $(TOPDIR)/kernel/Makefile.frag -C $(TOPDIR)/kernel all

# User

