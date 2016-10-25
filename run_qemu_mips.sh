#!/bin/sh

qemu-system-mipsel -S -s -M mipssim -m 32M -serial stdio -bios obj/bootloader/loader.bin -kernel obj/kernel/ucore-kernel-initrd
# qemu-system-mipsel -S -s -M mipssim -m 32M -serial stdio -bios obj/bootloader/loader.bin -kernel obj/kernel/ucore-kernel-initrd -d nochain,exec -D exc-nochain.log
# qemu-system-mipsel -S -s -M mipssim -m 32M -serial stdio -bios obj/bootloader/loader.bin -kernel obj/kernel/ucore-kernel-initrd -d exec -D exc.log

