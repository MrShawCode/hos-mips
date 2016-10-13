.PHONY: all clean

ULIB_OBJ_ROOT := $(OBJPATH_ROOT)/user
ULIB_A := $(ULIB_OBJ_ROOT)/ulib.a

TARGET_CFLAGS := -I. -Iinclude -Ilib -nostdinc -nostdlib -fno-builtin
obj-y := dir.o file.o malloc.o panic.o signal.o spipe.o \
				stdio.o string.o syscall.o thread.o ulib.o umain.o mod.o mount.o
obj-y += lib/hash.o lib/rand.o lib/printfmt.o lib/string.o

ifeq  ($(ON_FPGA), y)
MACH_DEF := -DMACH_FPGA
else
MACH_DEF := -DMACH_QEMU
endif

ARCH_CFLAGS := -mips1 -fno-builtin -nostdlib  -nostdinc -g  -EL -G0 -Wformat -O0 -mno-float $(MACH_DEF)
ARCH_LDFLAGS := 
ARCH_OBJS := syscall.o initcode.o intr.o clone.o udivmod.o udivmodsi4.o divmod.o
ARCH_INITCODE_OBJ := initcode.o

obj-y += $(ARCH_OBJS)
obj-initobj += $(ARCH_INITCODE_OBJ)
TARGET_CFLAGS += $(ARCH_CFLAGS)
TARGET_LDFLAGS += $(ARCH_LDFLAGS)

target-obj := $(addprefix $(ULIB_OBJ_ROOT)/, $(obj-y))
target-initobj := $(addprefix $(ULIB_OBJ_ROOT)/, $(obj-initobj))

all: $(target-initobj) $(ULIB_A)

$(ULIB_A): $(target-obj)
	$(TARGET_AR) -cr $@ $+

$(ULIB_OBJ_ROOT)/%.o: %.c | $(ULIB_OBJ_ROOT)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $@ $<

$(ULIB_OBJ_ROOT)/%.o: %.S | $(ULIB_OBJ_ROOT)
	$(TARGET_CC) -D__ASSEMBLY__ $(TARGET_CFLAGS) -c -o $@ $<

$(ULIB_OBJ_ROOT):
	@mkdir -p $(ULIB_OBJ_ROOT)/lib
	@mkdir -p $(ULIB_OBJ_ROOT)

clean:
	find . -name \*.o -exec rm -f {} \;
	rm -f ulib.a
