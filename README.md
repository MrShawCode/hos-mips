This is a simple mips architecture OS in windows platform based on Tsinghua ucore OS. 

For details about ucore, readers are refered to:
https://github.com/chyyuu/ucore_pub

This OS is built by using mips-sde-elf cross compiler (2014 version), and Cygwin (plus the build-in gcc tools in Cygwin) environment
(see http://www.cygwin.com/). Yes! the building can be conducted on Windows (Windows 7 and 10 are tested).

debug directory contains an useful app (portable PUTTY), bitstream file for Nexys4-DDR board, and many useful scripts (including vivado tcl scripts that burn the bitstream to board, and OpenOCD startup scripts). In order to use vivado script, the vivado bin path should be added into PATH.

After you set up the environment, you can just make and run the run.bat (UNCOMMENT the 2nd line to turn on/off the vivado work). The output will show in PUTTY.exe. (You may need to change the COM number in run.bat at line 3, this can be found by using PUTTY.EXE)

The .vscode directory gives an example about how to use GDB to debug the OS in Visual Studio Code with the Native Debug extension. You can just run the run.bat and setup a breakpoint. Then press "F5" to debug in vscode.

