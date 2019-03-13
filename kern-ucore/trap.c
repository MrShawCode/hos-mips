#include <defs.h>
#include <asm/mipsregs.h>
#include <clock.h>
#include <trap.h>
#include <arch.h>
#include <thumips_tlb.h>
#include <stdio.h>
#include <mmu.h>
#include <pmm.h>
#include <memlayout.h>
#include <pgmap.h>
#include <assert.h>
#include <console.h>
#include <monitor.h>
#include <error.h>
#include <syscall.h>
#include <proc.h>
#include <mips_io.h>

#define TICK_NUM 100

#define GET_CAUSE_EXCODE(x)   ( ((x) & CAUSEF_EXCCODE) >> CAUSEB_EXCCODE)

#define current (pls_read(current))
#define idleproc (pls_read(idleproc))

static void print_ticks()
{
	PRINT_HEX("%d ticks\n\r", TICK_NUM);
}

static const char *trapname(int trapno)
{
	static const char *const excnames[] = {
		"Interrupt",
		"TLB Modify",
		"TLB miss on load",
		"TLB miss on store",
		"Address error on load",
		"Address error on store",
		"Bus error on instruction fetch",
		"Bus error on data load or store",
		"Syscall",
		"Breakpoint",
		"Reserved (illegal) instruction",
		"Coprocessor unusable",
		"Arithmetic overflow",
	};
	if (trapno <= 12)
		return excnames[trapno];
	else
		return "Unknown";
}

bool trap_in_kernel(struct trapframe * tf)
{
	return !(tf->tf_status & KSU_USER);
}

void print_regs(struct pushregs *regs)
{
	int i;
	for (i = 0; i < 30; i++) {
		kprintf(" $");
		printbase10(i + 1);
		kprintf("\t: ");
		printhex(regs->reg_r[i]);
		kprintf("\n\r");
	}
}

void print_trapframe(struct trapframe *tf)
{
    PRINT_HEX("trapframe at ", tf);
    print_regs(&tf->tf_regs);
    PRINT_HEX("\r\n $ra: ", tf->tf_ra);
    PRINT_HEX("\r\n BadVA: ", tf->tf_vaddr);
    PRINT_HEX("\r\n Status: ", tf->tf_status);
    PRINT_HEX("\r\n Cause: ", tf->tf_cause);
    PRINT_HEX("\r\n EPC ", tf->tf_epc);
    if (!trap_in_kernel(tf)) {
        kprintf("\r\nTrap in usermode: ");
    } else {
        kprintf("\n\rTrap in kernel: ");
    }
    kprintf(trapname(GET_CAUSE_EXCODE(tf->tf_cause)));
    kprintf("\n\r");
}
//#define DEBUG_INT

static void interrupt_handler(struct trapframe *tf)
{
	extern clock_int_handler(void *);
	extern serial_int_handler(void *);
//        extern keyboard_int_handler();
	int i;
	for (i = 0; i < 8; i++) {
		if (tf->tf_cause & (1 << (CAUSEB_IP + i))) {
			switch (i) {
#ifdef DEBUG_INT
			//kprintf("INT - No %d\r\n", i);
#endif
			//case TIMER0_IRQ:
			case 7://corrected by xiaohan! Accoring to priority code.
				clock_int_handler(NULL);
				break;
			//case COM1_IRQ:
			case 6://corrected by xiaohan! Accoring to priority code.
				//kprintf("COM1\n");
				//kprintf("COM1\n\r");
				serial_int_handler(NULL);
				break;
//                        case KEYBOARD_IRQ:
//#ifdef DEBUG_INT
//                                kprintf("KEYBOARD\n");
//#endif
//                                pic_disable(KEYBOARD_IRQ);
//                                keyboard_int_handler();
//                                pic_enable(KEYBOARD_IRQ);
//                                break;
#ifdef BUILD_GXEMUL
			case 2:
			gxemul_input_intr();
			break;
#endif
			default:
				kprintf("[KERN] Received Unknown Interrupt:");
				print_trapframe(tf);
				panic("Unknown interrupt!");
			}
		}
	}

}

extern pde_t *current_pgdir;

static inline int get_error_code(int write, pte_t * pte)
{
	int r = 0;
	if (pte != NULL && ptep_present(pte))
		r |= 0x01;
	if (write)
		r |= 0x02;
	return r;
}

static int
pgfault_handler(struct trapframe *tf, uint32_t addr, uint32_t error_code)
{
	return do_pgfault(error_code, addr);
}

/* use software emulated X86 pgfault */
/* SZY comments: Do we have to dig this function out? If so, must heavily comment this function. */
/* SZY comments: encountered at least 1 pgfault when executing kernel, while 22 times of pgfault when invoking /bin/sh */
/* SZY comments: if we dig this function out, we must guarantee that there will be NO such fault generated during 
   the booting process (including the kernel loading and /bin/sh), and leave such senario to be handled when executing
   factorial program. Or else, the system just cannot be brought up. */

/* Hint:
 * 

 */
static void handle_tlbmiss(struct trapframe *tf, int write, int perm)//YX )
{
#if 0
	if (!trap_in_kernel(tf)) {
		print_trapframe(tf);
		while (1) ;
	}
#endif

	static int entercnt = 0;
	entercnt++;
//	kprintf("## enter handle_tlbmiss %d times\n\r", entercnt);

	int in_kernel = trap_in_kernel(tf);
	assert(current_pgdir != NULL);
	//print_trapframe(tf);
	uint32_t badaddr = tf->tf_vaddr;
	int ret = 0;
	pte_t *pte = get_pte(current_pgdir, tf->tf_vaddr, 0);
	//  )
	if (perm ||pte == NULL || ptep_invalid(pte) || (write && !ptep_u_write(pte))) {	//PTE miss, pgfault
		//panic("unimpl");
		//TODO
		//tlb will not be refill in do_pgfault,
		//so a vmm pgfault will trigger 2 exception
		//permission check in tlb miss
		ret = pgfault_handler(tf, badaddr, get_error_code(write, pte));
	} else {		//tlb miss only, reload it
		/* refill two slot */
		/* check permission */
		if (in_kernel) {
			tlb_refill(badaddr, pte);
			//kprintf("## refill K\n\r");
			return;
		} else {
			if (!ptep_u_read(pte)) {
				ret = -1;
				goto exit;
			}
			if (write && !ptep_u_write(pte)) {
				ret = -2;
				goto exit;
			}
			//kprintf("## refill U %d %08x\n\r", write, badaddr);
			tlb_refill(badaddr, pte);
			return;
		}
	}

exit:
	if (ret) {
		print_trapframe(tf);
		if (in_kernel) {
			panic("unhandled pgfault");
		} else {
			do_exit(-E_KILLED);
		}
	}
	return;
}

static void trap_dispatch(struct trapframe *tf)
{
	int i;
	int code = GET_CAUSE_EXCODE(tf->tf_cause);
	switch (code) {
	case EX_IRQ:
		interrupt_handler(tf);
		break;
	case EX_MOD:
		handle_tlbmiss(tf, 1, 1);
		break;
	case EX_TLBL:
		handle_tlbmiss(tf, 0, 0);//YX
		break;
	case EX_TLBS:
		handle_tlbmiss(tf, 1, 0);
		break;
	case EX_RI:
		print_trapframe(tf);
		uint32_t *addr = (uint32_t *) (tf->tf_epc);
		for (i = 0; i < 10; ++i, addr++)
			kprintf("[%x:%x]\n\r", addr, *addr);

		panic("hey man! Do NOT use that insn! insn=%x",
		      *(uint32_t *) (tf->tf_epc));
		break;
	case EX_SYS:
		//print_trapframe(tf);
		tf->tf_epc += 4;
		syscall();
		break;
		/* alignment error or access kernel
		 * address space in user mode */
	case EX_ADEL:
	case EX_ADES:
		if (trap_in_kernel(tf)) {
			print_trapframe(tf);
			panic("Alignment Error");
		} else {
			print_trapframe(tf);
			do_exit(-E_KILLED);
		}
		break;
	default:
		print_trapframe(tf);
		panic("Unhandled Exception");
	}

}

/*
 * General trap (exception) handling function for mips.
 * This is called by the assembly-language exception handler once
 * the trapframe has been set up.
 */
void mips_trap(struct trapframe *tf)
{
	static int flag = 0;
/* SZY comments: MUST add comments here to explain the conditions and branches */
	if (((tf->tf_cause >> 2) & 0x1F) != 0) {
//    kprintf(" [trap: epc=%x cause=%d syscall=%d badvaddr=%x pid=%d current_pgdir=%x] ", tf->tf_epc, (tf->tf_cause >> 2) & 0x1F, (unsigned)(tf->tf_regs.reg_r[MIPS_REG_V0]), tf->tf_vaddr, current?current->pid:-1, current_pgdir);
		flag = 1;
	} else
		flag = 0;

	// dispatch based on what type of trap occurred
	// used for previous projects
	if (current == NULL) {
		trap_dispatch(tf);
	} else {
		// keep a trapframe chain in stack
		struct trapframe *otf = current->tf;
		current->tf = tf;

		bool in_kernel = trap_in_kernel(tf);

		trap_dispatch(tf);

		current->tf = otf;
		if (!in_kernel) {
			if (current->flags & PF_EXITING) {
				do_exit(-E_KILLED);
			}
			if (current->need_resched) {
				schedule();
			}
		}
	}
//  if (flag) kprintf(" [end: epc=%x cause=%d syscall=%d pid=%d] ", tf->tf_epc, (tf->tf_cause >> 2) & 0x1F, (unsigned)(tf->tf_regs.reg_r[MIPS_REG_V0]), current?current->pid:-1);
}

//add by HHL
int ucore_in_interrupt()
{
	//panic("ucore_in_interrupt()");
	return 0;
}

int do_pgfault(machine_word_t error_code, uintptr_t addr)
{
	struct proc_struct *current = pls_read(current);

	int ret = -E_INVAL;

	assert(error_code == 3);

	pte_perm_t perm, nperm;
	ptep_unmap(&perm);
	ptep_set_u_read(&perm);
	addr = ROUNDDOWN(addr, PGSIZE);

	ret = -E_NO_MEM;

	pte_t *ptep;
	if ((ptep = get_pte(current->pgdir, addr, 1)) == NULL) {
		goto failed;
	}
	if (!(*ptep & PTE_COW)) {
		if (pgdir_alloc_page(current->pgdir, addr, perm) == NULL) {
			goto failed;
		}
	} else {		//a present page, handle copy-on-write (cow) 
		panic("unfinished");
	}
	ret = 0;

failed:
	return ret;
}
