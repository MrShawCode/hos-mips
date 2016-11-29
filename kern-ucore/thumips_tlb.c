/*
 * =====================================================================================
 *
 *       Filename:  thumips_tlb.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/06/2012 10:23:50 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Chen Yuheng (Chen Yuheng), chyh1990@163.com
 *   Organization:  Tsinghua Unv.
 *
 * =====================================================================================
 */
#include <defs.h>
#include <arch.h>
#include <stdio.h>
#include <string.h>
#include <mmu.h>
#include <memlayout.h>
#include <pmm.h>
#include <thumips_tlb.h>

// invalidate both TLB 
// (clean and flush, meaning we write the data back)
void tlb_invalidate(pde_t * pgdir, uintptr_t la)
{
	tlb_invalidate_all();
	return;
}

void tlb_invalidate_all()
{
    unsigned int i;
    unsigned int te;

    //kprintf("\n\rbegin tlb_invalidate_all()\n\r");
    //dump_tlb_all();
    for (i = 0; i < 16; i++)//
    {
        te=0x80000000 + (i << 20);
        //kprintf("te=0x%08x \n\r",te);
        write_one_tlb(i, 0, te, 0, 0);//13
    }
    //dump_tlb_all();
    //kprintf("\n\rend");

    return ;
}
