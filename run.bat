CD /D debug
:call program.bat	%NOTE: this command will overwrite your soft MIPS-core with standard core. Make sure you have vivado in PATH before using it%
start "" PUTTY.EXE -serial COM4 -sercfg 115200
start cmd /C loadMIPSfpga.bat ../obj/kernel/ucore-kernel-initrd
mips-sde-elf-gdb.exe ../obj/kernel/ucore-kernel-initrd -x startup-ucore.txt
CD /D ..
EXIT
