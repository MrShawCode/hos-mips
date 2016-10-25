#include <defs.h>
#include <arch.h>
#include <stdio.h>
#include <string.h>
#include <mmu.h>
#include <memlayout.h>
#include <pmm.h>
#include <default_pmm.h>
#include <sync.h>
#include <error.h>
#include <mips_io.h>
#include <proc.h>
#include <list.h>
#include <mp.h>
#include <thumips_tlb.h>
#include <mips_io.h>

// virtual address of physicall page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;

// virtual address of boot-time page directory
pde_t *boot_pgdir = NULL;

/* this is our emulated "CR3" */
pde_t *current_pgdir = NULL;

// physical address of boot-time page directory
uintptr_t boot_cr3;

// physical memory management
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

void lcr3(uintptr_t cr3)
{
	current_pgdir = (pde_t *) cr3;
}

//init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void)
{
	pmm_manager = &default_pmm_manager;
	kprintf("memory management: ");
	kprintf(pmm_manager->name);
	kprintf("\n\r");
	pmm_manager->init();
}

//init_memmap - call pmm->init_memmap to build Page struct for free memory  
static void init_memmap(struct Page *base, size_t n)
{
	pmm_manager->init_memmap(base, n);
}

//alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE memory 
struct Page *alloc_pages(size_t n)
{
	struct Page *page;
	bool intr_flag;
	local_intr_save(intr_flag);
	{
		page = pmm_manager->alloc_pages(n);
	}
	local_intr_restore(intr_flag);
	return page;
}

//free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory 
void free_pages(struct Page *base, size_t n)
{
	bool intr_flag;
	local_intr_save(intr_flag);
	{
		pmm_manager->free_pages(base, n);
	}
	local_intr_restore(intr_flag);
}

//nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE) 
//of current free memory
size_t nr_free_pages(void)
{
	size_t ret;
	bool intr_flag;
	local_intr_save(intr_flag);
	{
		ret = pmm_manager->nr_free_pages();
	}
	local_intr_restore(intr_flag);
	return ret;
}

/* pmm_init - initialize the physical memory management */
static void page_init(void)
{
	uint32_t maxpa;
	int i;

	//panic("unimpl");
	kprintf("memory map:\n\r");
	kprintf("    [0x");
	printhex(KERNBASE);
	kprintf(", 0x");
	printhex(KERNTOP);
	kprintf("]\n\r\n\r");

	maxpa = KERNTOP;
	npage = KMEMSIZE >> PGSHIFT;

	// end address of kernel
	extern char end[];
	// put page structure table at the end of kernel
	pages = (struct Page *)ROUNDUP_2N((void *)end, PGSHIFT);

	//reserve kernel pages, kernel space is reserved!
	for (i = 0; i < npage; i++) {
		SetPageReserved(pages + i);
	}

	uintptr_t freemem =
	    PADDR((uintptr_t) pages + sizeof(struct Page) * npage);
	kprintf( "freemem start at:%x\n\r", freemem );

	uint32_t mbegin = ROUNDUP_2N(freemem, PGSHIFT);
	uint32_t mend = ROUNDDOWN_2N(KERNTOP, PGSHIFT);
	assert(mbegin < mend);
	init_memmap(pa2page(mbegin), (mend - mbegin) >> PGSHIFT);
	PRINT_HEX("free pages: ", (mend - mbegin) >> PGSHIFT);
	PRINT_HEX("page structure size: ", sizeof(struct Page));
}

static void enable_paging(void)
{
	/* nothing */
	lcr3(boot_cr3);
}

//boot_map_segment - setup&enable the paging mechanism
// parameters
//  la:   linear address of this memory need to map (after x86 segment map)
//  size: memory size
//  pa:   physical address of this memory
//  perm: permission of this memory  
static void
boot_map_segment(pde_t * pgdir, uintptr_t la, size_t size, uintptr_t pa,
		 uint32_t perm)
{
	assert(PGOFF(la) == PGOFF(pa));
	size_t n = ROUNDUP_2N(size + PGOFF(la), PGSHIFT) >> PGSHIFT;
	la = ROUNDDOWN_2N(la, PGSHIFT);
	pa = ROUNDDOWN_2N(pa, PGSHIFT);
	for (; n > 0; n--, la += PGSIZE, pa += PGSIZE) {
		pte_t *ptep = get_pte(pgdir, la, 1);
		assert(ptep != NULL);
		*ptep = pa | PTE_P | perm;
	}
}

//boot_alloc_page - allocate one page using pmm->alloc_pages(1) 
// return value: the kernel virtual address of this allocated page
//note: this function is used to get the memory for PDT(Page Directory Table)&PT(Page Table)
static void *boot_alloc_page(void)
{
	struct Page *p = alloc_page();
	if (p == NULL) {
		panic("boot_alloc_page failed.\n\r");
	}
	return page2kva(p);
}

//pmm_init - setup a pmm to manage physical memory, build PDT&PT to setup paging mechanism 
//         - check the correctness of pmm & paging mechanism, print PDT&PT
void pmm_init(void)
{
	//We need to alloc/free the physical memory (granularity is 4KB or other size). 
	//So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
	//First we should init a physical memory manager(pmm) based on the framework.
	//Then pmm can alloc/free the physical memory. 
	//Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
	init_pmm_manager();

	// detect physical memory space, reserve already used memory,
	// then use pmm->init_memmap to create free page list
	page_init();

	//use pmm->check to verify the correctness of the alloc/free function in a pmm
	check_alloc_page();

	// create boot_pgdir, an initial page directory(Page Directory Table, PDT)
	boot_pgdir = boot_alloc_page();
	memset(boot_pgdir, 0, PGSIZE);
	boot_cr3 = PADDR(boot_pgdir);
	current_pgdir = boot_pgdir;

	check_pgdir();
	//FPGA
	boot_map_segment(boot_pgdir, KERNBASE, KMEMSIZE, 0, PTE_W);
	//temporary map:
	/* SZY comments: 1: 0x80000000 (KERNBASE) and up should be mapped to physical pages with exactly the same physical address? */
	/* 2: 0x80000000 = (KERNBASE) 2GB? 
	one root page table entry covers 1024 4KB pages, which means 4MB, low virtual address here. 
	what's your purpose here? */
	//virtual_addr 3G~3G+4M = linear_addr 0~4M = linear_addr 3G~3G+4M = phy_addr 0~4M
	boot_pgdir[0] = boot_pgdir[PDX(KERNBASE)];
	//YX FPGA	
	enable_paging();

	//YX
	/* SZY comments: again, you let kernel virtual address 0-4MB unmapped,why? explain*/
	boot_pgdir[0] = 0;
	//~YX
	//now the basic virtual memory map(see memlayout.h) is established.
	//check the correctness of the basic virtual memory map.
	check_boot_pgdir();

	memset(boot_pgdir, 0, PGSIZE);
	print_pgdir();
}

//page_remove_pte - free an Page sturct which is related linear address la
//                - and clean(invalidate) pte which is related linear address la
//note: PT is changed, so the TLB need to be invalidate 
static inline void page_remove_pte(pde_t * pgdir, uintptr_t la, pte_t * ptep)
{
	if (ptep && (*ptep & PTE_P)) {	// check if page directory is present
		struct Page *page = pte2page(*ptep);	// find corresponding page to pte
		// decrease page reference
		page_ref_dec(page);
		// and free it when reach 0
		if (page_ref(page) == 0) {
			free_page(page);
		}
		// clear page directory entry
		*ptep = 0;
	}
	// flush tlb
	tlb_invalidate_all();
}

static void check_alloc_page(void)
{
	pmm_manager->check();
	kprintf("check_alloc_page() succeeded!\n\r");
}

static void check_pgdir(void)
{
	assert(npage <= KMEMSIZE / PGSIZE);
	assert(boot_pgdir != NULL && (uint32_t) PGOFF(boot_pgdir) == 0);
	assert(get_page(boot_pgdir, 0x0, NULL) == NULL);

	struct Page *p1, *p2;
	p1 = alloc_page();
	assert(page_insert(boot_pgdir, p1, 0x0, 0) == 0);

	pte_t *ptep;
	assert((ptep = get_pte(boot_pgdir, 0x0, 0)) != NULL);
	assert(pa2page(*ptep) == p1);
	assert(page_ref(p1) == 1);

	ptep = &((pte_t *) KADDR(PDE_ADDR(boot_pgdir[0])))[1];
	assert(get_pte(boot_pgdir, PGSIZE, 0) == ptep);

	p2 = alloc_page();
	assert(page_insert(boot_pgdir, p2, PGSIZE, PTE_U | PTE_W) == 0);
	assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
	assert(*ptep & PTE_U);
	assert(*ptep & PTE_W);
	assert(boot_pgdir[0] & PTE_U);
	assert(page_ref(p2) == 1);

	assert(page_insert(boot_pgdir, p1, PGSIZE, 0) == 0);
	assert(page_ref(p1) == 2);
	assert(page_ref(p2) == 0);
	assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
	assert(pa2page(*ptep) == p1);
	assert((*ptep & PTE_U) == 0);

	page_remove(boot_pgdir, 0x0);
	assert(page_ref(p1) == 1);
	assert(page_ref(p2) == 0);

	page_remove(boot_pgdir, PGSIZE);
	assert(page_ref(p1) == 0);
	assert(page_ref(p2) == 0);

	assert(page_ref(pa2page(boot_pgdir[0])) == 1);
	free_page(pa2page(boot_pgdir[0]));
	boot_pgdir[0] = 0;

	kprintf("check_pgdir() succeeded!\n\r");
}

static void check_boot_pgdir(void)
{
	pte_t *ptep;
	int i;
	//assert(PDE_ADDR(boot_pgdir[PDX(VPT)]) == PADDR(boot_pgdir));

	assert(boot_pgdir[0] == 0);
	struct Page *p;
	p = alloc_page();
	*(int *)(page2kva(p) + 0x100) = 0x1234;
	//printhex(page2kva(p));
	//kprintf("\n");
	//printhex(*(int*)(page2kva(p)+0x100));

	assert(page_insert(boot_pgdir, p, 0x100, PTE_W) == 0);
	assert(page_ref(p) == 1);
	assert(page_insert(boot_pgdir, p, 0x100 + PGSIZE, PTE_W) == 0);
	assert(page_ref(p) == 2);

	//kprintf("\nHERE\n");

	assert(*(int *)0x100 == 0x1234);
	const char *str = "ucore: Hello world!!";
	strcpy((void *)0x100, str);
	assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

	*(char *)(page2kva(p) + 0x100) = '\0';
	assert(strlen((const char *)0x100) == 0);

	free_page(p);
	free_page(pa2page(PDE_ADDR(boot_pgdir[0])));
	boot_pgdir[0] = 0;
	tlb_invalidate_all();

	kprintf("check_boot_pgdir() succeeded!\n\r");
}

//perm2str - use string 'u,r,w,-' to present the permission
static const char *perm2str(int perm)
{
	static char str[4];
	str[0] = (perm & PTE_U) ? 'u' : '-';
	str[1] = 'r';
	str[2] = (perm & PTE_W) ? 'w' : '-';
	str[3] = '\0';
	return str;
}

//get_pgtable_items - In [left, right] range of PDT or PT, find a continuous linear addr space
//                  - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                  - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PGSIZE=4K, if PT
// paramemters:
//  left:        no use ???
//  right:       the high side of table's range
//  start:       the low side of table's range
//  table:       the beginning addr of table
//  left_store:  the pointer of the high side of table's next range
//  right_store: the pointer of the low side of table's next range
// return value: 0 - not a invalid item range, perm - a valid item range with perm permission
static int
get_pgtable_items(size_t left, size_t right, size_t start, uintptr_t * table,
		  size_t * left_store, size_t * right_store)
{
	if (start >= right) {
		return 0;
	}
	while (start < right && !(table[start] & PTE_P)) {
		start++;
	}
	if (start < right) {
		if (left_store != NULL) {
			*left_store = start;
		}
		int perm = (table[start++] & PTE_USER);
		while (start < right && (table[start] & PTE_USER) == perm) {
			start++;
		}
		if (right_store != NULL) {
			*right_store = start;
		}
		return perm;
	}
	return 0;
}

#define PRINT_PTE(s0, a0,a1,a2,a3,s1) kprintf(s0);printhex(a0);\
  kprintf(") ");printhex(a1);kprintf("-");printhex(a2);kprintf(" ");\
  printhex(a3);kprintf(" ");kprintf(s1);kprintf("\n\r");
//print_pgdir - print the PDT&PT
void print_pgdir(void)
{
	size_t left, right = 0, perm;
	kprintf("---------------- PAGE Directory BEGIN ----------------\n\r");
/* SZY comments: why this shows empty in QEMU emulator? */
	while ((perm =
		get_pgtable_items(0, NPDEENTRY, right, current_pgdir, &left,
				  &right)) != 0) {
		PRINT_PTE("PDE(", right - left, left * PTSIZE, right * PTSIZE,
			  (right - left) * PTSIZE, perm2str(perm));
		size_t l, r = 0;

		size_t perm_ref = get_pgtable_items(0, NPTEENTRY, r,
						    (pte_t *)
						    PDE_ADDR(current_pgdir
							     [left]),
						    &l, &r);
		size_t count, count_ref = 0;
		size_t count_ref_l = 0;
		for (count = 0; count < right - left; count++) {
			l = r = 0;
			while ((perm =
				get_pgtable_items(0, NPTEENTRY, r, (pte_t *)
						  PDE_ADDR(current_pgdir
							   [left + count]), &l,
						  &r)) != 0) {
				if (perm != perm_ref
				    || count == right - left - 1) {
					size_t total_entries =
					    (count - count_ref -
					     1) * NPTEENTRY + (r - l) +
					    (NPTEENTRY - count_ref_l);
					PRINT_PTE("  |-- PTE(", total_entries,
						  (left + count_ref) * PTSIZE +
						  count_ref_l * PGSIZE,
						  (left + count) * PTSIZE +
						  r * PGSIZE,
						  total_entries * PGSIZE,
						  perm2str(perm_ref));
					perm_ref = perm;
					count_ref = count;
					count_ref_l = r;
				}
			}
		}
	}
	kprintf("---------------- PAGE Directory END ------------------\n\r");
}

//added by HHL

void load_rsp0(uintptr_t esp0)
{

}

void map_pgdir(pde_t * pgdir)
{
#define VPT                 0xFAC00000
	pgdir[PDX(VPT)] = PADDR(pgdir) | PTE_P | PTE_W;
}

/**
 * set_pgdir - save the physical address of the current pgdir
 */
void set_pgdir(struct proc_struct *proc, pde_t * pgdir)
{
	assert(proc != NULL);
	proc->cr3 = PADDR(pgdir);
}

size_t nr_used_pages(void)
{
	return 0;
}

#include <pmm.h>
#include <string.h>
#include <error.h>
#include <memlayout.h>
#include <mp.h>

/**************************************************
 * Page table operations
 **************************************************/

pgd_t *get_pgd(pgd_t * pgdir, uintptr_t la, bool create)
{
	return &pgdir[PGX(la)];
}

pud_t *get_pud(pgd_t * pgdir, uintptr_t la, bool create)
{
#if PUXSHIFT == PGXSHIFT
	return get_pgd(pgdir, la, create);
#else /* PUXSHIFT == PGXSHIFT */
	pgd_t *pgdp;
	if ((pgdp = get_pgd(pgdir, la, create)) == NULL) {
		return NULL;
	}
	if (!ptep_present(pgdp)) {
		struct Page *page;
		if (!create || (page = alloc_page()) == NULL) {
			return NULL;
		}
		set_page_ref(page, 1);
		uintptr_t pa = page2pa(page);
		memset(KADDR(pa), 0, PGSIZE);
		ptep_map(pgdp, pa);
		ptep_set_u_write(pgdp);
		ptep_set_accessed(pgdp);
		ptep_set_dirty(pgdp);
	}
	return &((pud_t *) KADDR(PGD_ADDR(*pgdp)))[PUX(la)];
#endif /* PUXSHIFT == PGXSHIFT */
}

pmd_t *get_pmd(pgd_t * pgdir, uintptr_t la, bool create)
{
#if PMXSHIFT == PUXSHIFT
	return get_pud(pgdir, la, create);
#else /* PMXSHIFT == PUXSHIFT */
	pud_t *pudp;
	if ((pudp = get_pud(pgdir, la, create)) == NULL) {
		return NULL;
	}
	if (!ptep_present(pudp)) {
		struct Page *page;
		if (!create || (page = alloc_page()) == NULL) {
			return NULL;
		}
		set_page_ref(page, 1);
		uintptr_t pa = page2pa(page);
		memset(KADDR(pa), 0, PGSIZE);
		ptep_map(pudp, pa);
		ptep_set_u_write(pudp);
		ptep_set_accessed(pudp);
		ptep_set_dirty(pudp);
	}
	return &((pmd_t *) KADDR(PUD_ADDR(*pudp)))[PMX(la)];
#endif /* PMXSHIFT == PUXSHIFT */
}

pte_t *get_pte(pgd_t * pgdir, uintptr_t la, bool create)
{
/* SZY comments: PMXSHIFT seems undefined here! Possibly, the compare of PTXSHIFT and PMXSHIFT directly produce false */
#if PTXSHIFT == PMXSHIFT
	printk( "PMXSHIFT is defined as:0x%x\n\r", PMXSHIFT );
	return get_pmd(pgdir, la, create);
#else /* PTXSHIFT == PMXSHIFT */
	pmd_t *pmdp;
	if ((pmdp = get_pmd(pgdir, la, create)) == NULL) {
		if(pmdp == NULL)
		 kprintf("pmdp == NULL\n");
	
		return NULL;
	}
	
	if (!ptep_present(pmdp)) {
		struct Page *page;
		if (!create || (page = alloc_page()) == NULL) {
			return NULL;
		}
		set_page_ref(page, 1);
		uintptr_t pa = page2pa(page);
		memset(KADDR(pa), 0, PGSIZE);
		ptep_map(pmdp, pa);
		ptep_set_u_write(pmdp);
		ptep_set_accessed(pmdp);
		ptep_set_dirty(pmdp);
	}
	return &((pte_t *) KADDR(PMD_ADDR(*pmdp)))[PTX(la)];
#endif /* PTXSHIFT == PMXSHIFT */
}

/**
 * get related Page struct for linear address la using PDT pgdir
 * @param pgdir page directory
 * @param la linear address
 * @param ptep_store table entry stored if not NULL
 * @return @la's corresponding page descriptor
 */
struct Page *get_page(pgd_t * pgdir, uintptr_t la, pte_t ** ptep_store)
{
	pte_t *ptep = get_pte(pgdir, la, 0);
	if (ptep_store != NULL) {
		*ptep_store = ptep;
	}
	if (ptep != NULL && ptep_present(ptep)) {
		return pa2page(*ptep);
	}
	return NULL;
}

/**
 * page_insert - build the map of phy addr of an Page with the linear addr @la
 * @param pgdir page directory
 * @param page the page descriptor of the page to be inserted
 * @param la logical address of the page
 * @param perm permission of the page
 * @return 0 on success and error code when failed
 */
int page_insert(pgd_t * pgdir, struct Page *page, uintptr_t la, pte_perm_t perm)
{
	pte_t *ptep = get_pte(pgdir, la, 1);
	if (ptep == NULL) {
		return -E_NO_MEM;
	}
	page_ref_inc(page);
	if (*ptep != 0) {
		if (ptep_present(ptep) && pte2page(*ptep) == page) {
			page_ref_dec(page);
			goto out;
		}
		page_remove_pte(pgdir, la, ptep);
	}

out:
	ptep_map(ptep, page2pa(page));
	ptep_set_perm(ptep, perm);
	mp_tlb_update(pgdir, la);
	return 0;
}

/**
 * page_remove - free an Page which is related linear address la and has an validated pte
 * @param pgdir page directory
 * @param la logical address of the page to be removed
 */
void page_remove(pgd_t * pgdir, uintptr_t la)
{
	pte_t *ptep = get_pte(pgdir, la, 0);
	if (ptep != NULL) {
		page_remove_pte(pgdir, la, ptep);
	}
}

/**
 * pgdir_alloc_page - call alloc_page & page_insert functions to 
 *                  - allocate a page size memory & setup an addr map
 *                  - pa<->la with linear address la and the PDT pgdir
 * @param pgdir    page directory
 * @param la       logical address for the page to be allocated
 * @param perm     permission of the page
 * @return         the page descriptor of the allocated
 */
struct Page *pgdir_alloc_page(pgd_t * pgdir, uintptr_t la, uint32_t perm)
{
	struct Page *page = alloc_page();
	if (page != NULL) {
		//zero it!
		memset(page2kva(page), 0, PGSIZE);
		if (page_insert(pgdir, page, la, perm) != 0) {
			free_page(page);
			return NULL;
		}
	}
	return page;
}

/**************************************************
 * memory management for users
 **************************************************/

static void
unmap_range_pte(pgd_t * pgdir, pte_t * pte, uintptr_t base, uintptr_t start,
		uintptr_t end)
{
	assert(start >= 0 && start < end && end <= PTSIZE);
	assert(start % PGSIZE == 0 && end % PGSIZE == 0);
	do {
		pte_t *ptep = &pte[PTX(start)];
		if (*ptep != 0) {
			page_remove_pte(pgdir, base + start, ptep);
		}
		start += PGSIZE;
	} while (start != 0 && start < end);
}

static void
unmap_range_pmd(pgd_t * pgdir, pmd_t * pmd, uintptr_t base, uintptr_t start,
		uintptr_t end)
{
#if PMXSHIFT == PUXSHIFT
	unmap_range_pte(pgdir, pmd, base, start, end);
#else
	assert(start >= 0 && start < end && end <= PMSIZE);
	size_t off, size;
	uintptr_t la = ROUNDDOWN(start, PTSIZE);
	do {
		off = start - la, size = PTSIZE - off;
		if (size > end - start) {
			size = end - start;
		}
		pmd_t *pmdp = &pmd[PMX(la)];
		if (ptep_present(pmdp)) {
			unmap_range_pte(pgdir, KADDR(PMD_ADDR(*pmdp)),
					base + la, off, off + size);
		}
		start += size, la += PTSIZE;
	} while (start != 0 && start < end);
#endif
}

static void
unmap_range_pud(pgd_t * pgdir, pud_t * pud, uintptr_t base, uintptr_t start,
		uintptr_t end)
{
#if PUXSHIFT == PGXSHIFT
	unmap_range_pmd(pgdir, pud, base, start, end);
#else
	assert(start >= 0 && start < end && end <= PUSIZE);
	size_t off, size;
	uintptr_t la = ROUNDDOWN(start, PMSIZE);
	do {
		off = start - la, size = PMSIZE - off;
		if (size > end - start) {
			size = end - start;
		}
		pud_t *pudp = &pud[PUX(la)];
		if (ptep_present(pudp)) {
			unmap_range_pmd(pgdir, KADDR(PUD_ADDR(*pudp)),
					base + la, off, off + size);
		}
		start += size, la += PMSIZE;
	} while (start != 0 && start < end);
#endif
}

static void unmap_range_pgd(pgd_t * pgd, uintptr_t start, uintptr_t end)
{
	size_t off, size;
	uintptr_t la = ROUNDDOWN(start, PUSIZE);
	do {
		off = start - la, size = PUSIZE - off;
		if (size > end - start) {
			size = end - start;
		}
		pgd_t *pgdp = &pgd[PGX(la)];
		if (ptep_present(pgdp)) {
			unmap_range_pud(pgd, KADDR(PGD_ADDR(*pgdp)), la, off,
					off + size);
		}
		start += size, la += PUSIZE;
	} while (start != 0 && start < end);
}

void unmap_range(pgd_t * pgdir, uintptr_t start, uintptr_t end)
{
	assert(start % PGSIZE == 0 && end % PGSIZE == 0);
	assert(USER_ACCESS(start, end));
	unmap_range_pgd(pgdir, start, end);
}

static void exit_range_pmd(pmd_t * pmd)
{
#if PMXSHIFT == PUXSHIFT
	/* do nothing */
#else
	uintptr_t la = 0;
	do {
		pmd_t *pmdp = &pmd[PMX(la)];
		if (ptep_present(pmdp)) {
			free_page(pmd2page(*pmdp)), *pmdp = 0;
		}
		la += PTSIZE;
	} while (la != PMSIZE);
#endif
}

static void exit_range_pud(pud_t * pud)
{
#if PUXSHIFT == PGXSHIFT
	exit_range_pmd(pud);
#else
	uintptr_t la = 0;
	do {
		pud_t *pudp = &pud[PUX(la)];
		if (ptep_present(pudp)) {
			exit_range_pmd(KADDR(PUD_ADDR(*pudp)));
			free_page(pud2page(*pudp)), *pudp = 0;
		}
		la += PMSIZE;
	} while (la != PUSIZE);
#endif
}

static void exit_range_pgd(pgd_t * pgd, uintptr_t start, uintptr_t end)
{
	start = ROUNDDOWN(start, PUSIZE);
	do {
		pgd_t *pgdp = &pgd[PGX(start)];
		if (ptep_present(pgdp)) {
			exit_range_pud(KADDR(PGD_ADDR(*pgdp)));
			free_page(pgd2page(*pgdp)), *pgdp = 0;
		}
		start += PUSIZE;
	} while (start != 0 && start < end);
}

void exit_range(pgd_t * pgdir, uintptr_t start, uintptr_t end)
{
	assert(start % PGSIZE == 0 && end % PGSIZE == 0);
	assert(USER_ACCESS(start, end));
	exit_range_pgd(pgdir, start, end);
}

/* copy_range - copy content of memory (start, end) of one process A to another process B
 * @to:    the addr of process B's Page Directory
 * @from:  the addr of process A's Page Directory
 * @share: flags to indicate to dup OR share. We just use dup method, so it didn't be used.
 *
 * CALL GRAPH: copy_mm-->dup_mmap-->copy_range
 */
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share) {
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));
    // copy content by page unit.
    do {
        //call get_pte to find process A's pte according to the addr start
        pte_t *ptep = get_pte(from, start, 0), *nptep;
        if (ptep == NULL) {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue ;
        }
        //call get_pte to find process B's pte according to the addr start. If pte is NULL, just alloc a PT
        if (*ptep & PTE_P) {
            if ((nptep = get_pte(to, start, 1)) == NULL) {
                return -E_NO_MEM;
            }
        uint32_t perm = (*ptep & PTE_USER);
        //get page from ptep
        struct Page *page = pte2page(*ptep);
        // alloc a page for process B
        struct Page *npage=alloc_page();
        assert(page!=NULL);
        assert(npage!=NULL);
        int ret=0;
        /*   
         * (1) find src_kvaddr: the kernel virtual address of page
         * (2) find dst_kvaddr: the kernel virtual address of npage
         * (3) memory copy from src_kvaddr to dst_kvaddr, size is PGSIZE
         * (4) build the map of phy addr of  nage with the linear addr start
         */
        void * kva_src = page2kva(page);
        void * kva_dst = page2kva(npage);
    
        memcpy(kva_dst, kva_src, PGSIZE);

        ret = page_insert(to, npage, start, perm);
        assert(ret == 0);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
    return 0;
}


#define MAX_SIZE 1024 * 1024 * 1024 / PGSIZE
void *ptrs[MAX_SIZE];
size_t sizes[MAX_SIZE];
int index = 0;

void *
kmalloc(size_t n) {
    void *ptr=NULL;
    struct Page *base=NULL;
	assert(n > 0 && n < 1024*1024);
    int num_pages=(n+PGSIZE-1)/PGSIZE;
    base = alloc_pages(num_pages);
    assert(base != NULL);
    ptr=page2kva(base);
	ptrs[index] = ptr;
	sizes[index] = n;
	index++;
	assert(index < MAX_SIZE);
    return ptr;
}

void 
kfree(void *ptr) {
	size_t n = 0;
	int i;
	for(i = 0; i < index; i++){
		if(ptrs[i] == ptr){
			ptrs[i] = ptrs[index - 1];
			index--;
			n = sizes[i];
			break;
		}
	}
	if(n >= 1024 * 1024 || n <= 0)
		kprintf("%d %d %d\n\r",i, index, n);
    assert(n > 0 && n < 1024*1024);
    assert(ptr != NULL);
    struct Page *base=NULL;
    int num_pages=(n+PGSIZE-1)/PGSIZE;
    base = kva2page(ptr);
    free_pages(base, num_pages);
}
