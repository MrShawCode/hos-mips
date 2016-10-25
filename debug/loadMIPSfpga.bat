:: File: loadMIPSfpga.bat
:: 
:: Description: Compiles program, creates memory files for simulation/synthesis, and
:: 	loads program onto MIPSfpga using Open OCD and Codescape Essentials (gdb)
::
:: DATE:   7-FEB-2015

@ECHO OFF

SETLOCAL

set indir=%1%
if "%indir%"=="" (
  @echo ERROR: You must enter the program directory. & @echo. & @echo Example: & @echo loadMIPSfpga.bat ..\..\Module02_C\ReadSwitches & exit /b
) else ( 
	set indir=%indir%\
)

set gdbStartupFile=%CD%\..\startup.txt
:: @echo gdbStartupFile: %gdbStartupFile%

set infile=%indir%FPGA_Ram.elf

set Pathname_OCD="C:\Program Files\Imagination Technologies\OpenOCD\OpenOCD-0.9.2\bin\"

start /wait cmd.exe /C  "cd /d %indir% && make"
start "OpenOCD" cmd.exe /K "%Pathname_OCD%openocd-0.9.2.exe"
::start "gdb" cmd.exe /K "cd /d %indir% && mips-sde-elf-gdb -q FPGA_Ram.elf -x %gdbStartupFile%"

EXIT /B 1

