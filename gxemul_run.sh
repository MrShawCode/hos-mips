#!/bin/bash
gxemul -E oldtestmips -C R6000 obj/kernel/ucore-kernel-initrd -M 512 $@
