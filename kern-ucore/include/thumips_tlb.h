/*
 * =====================================================================================
 *
 *       Filename:  thumips_tlb.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/06/2012 10:20:30 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Chen Yuheng (Chen Yuheng), chyh1990@163.com
 *   Organization:  Tsinghua Unv.
 *
 * =====================================================================================
 */
#ifndef _THUMIPS_TLB_H
#define _THUMIPS_TLB_H

#include <asm/mipsregs.h>
#include <memlayout.h>
#include <pgmap.h>
#include <mips_io.h>

#define THUMIPS_TLB_ENTRYL_V (1<<1)
#define THUMIPS_TLB_ENTRYL_D (1<<2)
#define THUMIPS_TLB_ENTRYL_G (1<<0)
#define THUMIPS_TLB_ENTRYH_VPN2_MASK (~0x1FFF)

#define PAGE_MASK (0xffffffff<<12)
static void dump_tlb_all()
{
    unsigned int pagemask, c0, c1, asid;
    unsigned int entrylo0 = 0, entrylo1 = 0;
    unsigned int entryhi = 0;
    int i = 0;
/* save current asid */
    asm (
        "mfc0 %0, $10\n\t"
        : "=r" (asid)
    );
    asid &= 0xff;
    kprintf("\n\r");
    for (; i <=16 - 1; i++) {
    /* read a TLB Entry*/
        write_c0_index(i);
        tlb_read();
        pagemask= read_c0_pagemask();
        entryhi=read_c0_entryhi();
        entrylo0=read_c0_entrylo0();
        entrylo1=read_c0_entrylo1();
        //asm (
        //  "mtc0 %4, $0\n\t"/* write i to Index */
        //  "tlbr\n\t" /* read the i tlb entry to register*/
        //  "mfc0 %0, $5\n\t"/* read PageMask */
        //  "mfc0 %1, $10\n\t" /* read EntryHi */
        //  "mfc0 %2, $2\n\t"/* read EntryLo0 */
        //  "mfc0 %3, $3\n\t"/* read EntryLo1 */
        //  : "=r" (pagemask), "=r" (entryhi), "=r" (entrylo0), "=r" (entrylo1)
        //  : "Jr" (i)
        //);
/* print its content */
        kprintf("Index: %2d pgmask=0x%08x \n\r", i, pagemask);
        kprintf("entryhi: 0x%08x entrylo0=0x%08x entrylo1=0x%08x \n\r", entryhi, entrylo0,entrylo1);

        c0 = (entrylo0 >> 3) & 7;
        c1 = (entrylo1 >> 3) & 7;
        kprintf("va=%08x asid=%02x\n\r",
        (entryhi & 0xffffe000), (entryhi & 0xff));
        kprintf("\t\t\t[pa=%08x c=%d d=%d v=%d g=%d]\n\r",
        (entrylo0 << 6)& PAGE_MASK, c0,
        (entrylo0 & 4) ? 1 : 0,
        (entrylo0 & 2) ? 1 : 0, (entrylo0 & 1));
        kprintf("\t\t\t[pa=%08x c=%d d=%d v=%d g=%d]\n\r",
        (entrylo1 << 6)& PAGE_MASK, c1,
        (entrylo1 & 4) ? 1 : 0,
        (entrylo1 & 2) ? 1 : 0, (entrylo1 & 1));
        kprintf("\n\r");
    }
/* restore asid */
    asm (
        "mtc0 %0, $10\n\t"
        : : "Jr" (asid)
    );
}

static inline void write_one_tlb(int index, unsigned int pagemask,
				 unsigned int hi, unsigned int low0,
				 unsigned int low1)
{
	write_c0_entrylo0(low0);
	write_c0_pagemask(pagemask);
	write_c0_entrylo1(low1);
	write_c0_entryhi(hi);
	write_c0_index(index);
	tlb_write_indexed();
}

static inline void tlb_replace_random(unsigned int pagemask, unsigned int hi,
				      unsigned int low0, unsigned int low1)
{
	write_c0_entrylo0(low0);
	write_c0_pagemask(pagemask);
	write_c0_entrylo1(low1);
	write_c0_entryhi(hi);
	tlb_write_random();
}

static inline uint32_t pte2tlblow(pte_t pte)
{
	uint32_t t = (((uint32_t) pte - 0x80000000) >> 12) << 6;
	if (!ptep_present(&pte))
		return 0;
	t |= THUMIPS_TLB_ENTRYL_V;
	/* always ignore ASID */
	t |= THUMIPS_TLB_ENTRYL_G;
	t |= (3 << 3);
	if (ptep_s_write(&pte))
		t |= THUMIPS_TLB_ENTRYL_D;
	return t;
}

static inline void tlb_refill(uint32_t badaddr, pte_t * pte)
{
    if (!pte)
        return;
    if (badaddr & (1 << 12))
        pte--;
    //kprintf("*pte=0x%08x\n\r",*pte);
    //kprintf("*pte+1=0x%08x\n\r",*(pte+1));
//#ifdef MACH_QEMU
    //kprintf("badaddr=0x%08x\n\r",badaddr);
    tlb_replace_random(0, badaddr & THUMIPS_TLB_ENTRYH_VPN2_MASK,
               pte2tlblow(*pte), pte2tlblow(*(pte + 1)));
/*#elif defined MACH_FPGA
    //TODO
    static int index = 0;
    write_one_tlb(index++/*(badaddr >> 13), 0, badaddr & THUMIPS_TLB_ENTRYH_VPN2_MASK, pte2tlblow(*pte), pte2tlblow(*(pte+1)));
#endif*/

}

void tlb_invalidate_all();

#endif
