#!/bin/sh

qemu-system-mipsel -S -s -M mipssim -m 32M -serial stdio -bios obj/bootloader/loader.bin -kernel obj/kernel/ucore-kernel-initrd
