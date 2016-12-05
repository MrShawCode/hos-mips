ifeq  ($(ON_FPGA), y)
MACH_DEF := -DMACH_FPGA
else
MACH_DEF := -DMACH_QEMU
endif

ARCH_CFLAGS := -g $(MACH_DEF) -fno-builtin -nostdlib  -nostdinc -EL -mno-mips16  -msoft-float -march=m14k -G0 -Wformat -O0 -msoft-float
ARCH_LDFLAGS := -n -G 0 -static -EL -nostdlib

