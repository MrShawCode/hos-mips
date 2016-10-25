CD /D debug
call program.bat
start cmd /C PUTTY.EXE -serial COM4 -sercfg 115200
start cmd /C loadMIPSfpga.bat ../obj/kernel/ucore-kernel-initrd
REM mips-sde-elf-gdb.exe ../obj/kernel/ucore-kernel-initrd -x startup-ucore.txt
CD /D ..
EXIT