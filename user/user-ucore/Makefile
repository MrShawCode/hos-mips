.PHONY: all clean

-include $(KCONFIG_AUTOCONFIG)

TARGET_CFLAGS:= 	-I../include -I../libs -nostdinc -nostdlib -fno-builtin

#TARGET_CC_SYSTEM_LIB ?=  -L/opt/FriendlyARM/toolschain/4.4.3/lib/gcc/arm-none-linux-gnueabi/4.4.3/ 

USER_OBJ_ROOT := $(OBJPATH_ROOT)/user-ucore

USER_LIB_SRCDIR := ..
USER_LIB_OBJDIR := $(OBJPATH_ROOT)/user
USER_LIB := $(USER_LIB_OBJDIR)/ulib.a
BIN := $(USER_OBJ_ROOT)/bin
TESTBIN := $(USER_OBJ_ROOT)/testbin
INITIAL_DIR := _initial

# USER_APPLIST:= pwd cat sh ls cp echo num #link mkdir rename unlink lsmod insmod rmmod mount umount
ifneq ($(UCORE_TEST),)
USER_TESTLIST := $(basename $(wildcard tests/*.c))
USER_TESTLIST += $(basename $(wildcard tests/*.c))
endif

USER_APP_BINS:= $(addprefix $(BIN)/, $(USER_APPLIST))
USER_TEST_BINS := $(addprefix $(TESTBIN)/, $(notdir $(USER_TESTLIST)))

ifeq  ($(ON_FPGA), y)
MACH_DEF := -DMACH_FPGA
else
MACH_DEF := -DMACH_QEMU
endif

ARCH_CFLAGS := -mips1 -fno-builtin -nostdlib  -nostdinc -g  -EL -G0 -Wformat -O0 -mno-float $(MACH_DEF)
ARCH_LDFLAGS := 
TARGET_CFLAGS += $(ARCH_CFLAGS)
TARGET_LDFLAGS += $(ARCH_LDFLAGS)

all:  $(BIN) $(USER_LIB) $(USER_APP_BINS)

#user applications
define make-user-app
#$1: $(BUILD_DIR) $(addsuffix .o,$1) $(USER_LIB)
$2/$(notdir $1): $(USER_OBJ_ROOT)/$(addsuffix .o,$1) $(INITCODE_OBJ) $(USER_LIB)
	@echo LINK $$@
	$(TARGET_LD) $(TARGET_LDFLAGS) -static -T $(USER_LIB_SRCDIR)/user.ld $(USER_OBJ_ROOT)/$(addsuffix .o,$1) $(INITCODE_OBJ) $(USER_LIB) -o $$@
#	sed 's/$$$$FILE/$(notdir $1)/g' tools/piggy.S.in > $(USER_OBJ_ROOT)/piggy.S
#	$(CROSS_COMPILE)as $(USER_OBJ_ROOT)/piggy.S -o $$@.piggy.o
endef

$(foreach bdir,$(USER_APPLIST),$(eval $(call make-user-app,$(bdir),$(BIN))))

$(foreach bdir,$(USER_TESTLIST),$(eval $(call make-user-app,$(bdir),$(TESTBIN))))

$(USER_OBJ_ROOT)/%.o: %.c
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $@ $<


$(BIN):
	-mkdir -p $(BIN)

clean:
	find . -name \*.o -exec rm -f {} \;
	rm -rf $(BIN) $(TESTBIN)
