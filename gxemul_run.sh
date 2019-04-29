#!/bin/bash
gxemul -E oldtestmips -C R6000 -M 512 obj/kernel/ucore-kernel-initrd $@
