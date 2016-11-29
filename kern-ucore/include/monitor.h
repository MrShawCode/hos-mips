#ifndef __KERN_DEBUG_MONITOR_H__
#define __KERN_DEBUG_MONITOR_H__

#include <trap.h>

void monitor(struct trapframe *tf);
void print_kerninfo(void);

int mon_help(int argc, char **argv, struct trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct trapframe *tf);

#endif /* !__KERN_DEBUG_MONITOR_H__ */
