#ifndef __UCORE_MP_H__
#define __UCORE_MP_H__

#include <mp.h>
#include <memlayout.h>
#include <types.h>

#define LAPIC_COUNT 1

#define PLS

#define pls_read(var) pls_##var

#define pls_get_ptr(var) &pls_##var

#define pls_write(var, value)											\
	do {																\
		pls_##var = value;                                              \
	} while (0)

extern int pls_lapic_id;
extern int pls_lcpu_idx;
extern int pls_lcpu_count;

extern volatile int ipi_raise[LAPIC_COUNT];

extern pgd_t *mpti_pgdir;
extern uintptr_t mpti_la;
extern volatile int mpti_end;

int mp_init(void);

void kern_enter(int source);
void kern_leave(void);
void set_pagetable(pgd_t * pgdir);
void __mp_tlb_invalidate(pgd_t * pgdir, uintptr_t la);
void mp_tlb_invalidate(pgd_t * pgdir, uintptr_t la);
void mp_tlb_update(pgd_t * pgdir, uintptr_t la);

#endif
