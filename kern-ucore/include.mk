ifeq  ($(ON_FPGA), y)
MACH_DEF := -DMACH_FPGA
else
MACH_DEF := -DMACH_QEMU
endif

ARCH_CFLAGS := -mips1 $(MACH_DEF) -fno-builtin -nostdlib  -nostdinc -g  -EL -G0 -Wformat -O2 -mno-float
ARCH_LDFLAGS := -n -G 0 -static -EL -nostdlib

