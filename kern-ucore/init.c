#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <monitor.h>
#include <picirq.h>
#include <trap.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <proc.h>
#include <thumips_tlb.h>
#include <sched.h>

void __noreturn kern_init(void)
{
	tlb_invalidate_all();
	char *p = 0x7ffff000;
	// imzhwk: the whole MP code is garbage.
	// dont get why dont just delete it.
	// anyway.
	mp_init();
	pic_init();		// init interrupt controller
	cons_init();		// init the console
	clock_init();		// init clock interrupt
	kprintf("imzhwk: Disabling interrupts to perform\n       initialization process without noises..\n");
	intr_disable();     // disable interrupt

	check_initrd();

	const char *message = "Welcome to HOS!\n\r\n\r";
	kprintf(message);
	print_kerninfo();

	pmm_init();		// init physical memory management

	sched_init();
	proc_init();		// init process table
	ide_init();
	fs_init();
	intr_enable();		// enable irq interrupt
	cpu_idle();
}
