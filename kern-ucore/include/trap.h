#ifndef __KERN_TRAP_TRAP_H__
#define __KERN_TRAP_TRAP_H__

#include <defs.h>
#include <mips_trapframe.h>
#include <mips_io.h>
#include <pmm.h>

void print_trapframe(struct trapframe *tf);
void print_regs(struct pushregs *regs);
bool trap_in_kernel(struct trapframe *tf);
int ucore_in_interrupt();
int do_pgfault(machine_word_t error_code, uintptr_t addr);

#endif /* !__KERN_TRAP_TRAP_H__ */
