#include <mp.h>
#include <proc.h>
#include <pmm.h>

PLS int pls_lapic_id;
PLS int pls_lcpu_idx;
PLS int pls_lcpu_count;
void set_pagetable(pgd_t *pgdir)
{
	if (pgdir != NULL)
		lcr3(PADDR(pgdir));
	else
		lcr3(boot_cr3);
	tlb_invalidate_all();
}

void mp_tlb_invalidate(pgd_t * pgdir, uintptr_t la)
{
	tlb_invalidate_all();
}

void mp_tlb_update(pgd_t * pgdir, uintptr_t la)
{
	tlb_invalidate_all();
}

int mp_init(void)
{
	pls_write(lapic_id, 0);
	pls_write(lcpu_idx, 0);
	pls_write(lcpu_count, 1);

	return 0;
}
