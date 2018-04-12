#include <defs.h>
#include <assert.h>
#include <arch.h>
#include <picirq.h>
#include <asm/mipsregs.h>



#define INTC_BASE 0xB0200000
#define INTC_ISR 0x00			/* Interrupt Status Register */
#define INTC_IPR 0x04			/* Interrupt Pending Register */
#define INTC_IER 0x08			/* Interrupt Enable Register */
#define INTC_IAR 0x0c			/* Interrupt Acknowledge Register */
#define INTC_SIE 0x10			/* Set Interrupt Enable bits */
#define INTC_CIE 0x14			/* Clear Interrupt Enable bits */
#define INTC_IVR 0x18			/* Interrupt Vector Register */
#define INTC_MER 0x1c			/* Master Enable Register */

#define INTC_MER_ME (1<<0)
#define INTC_MER_HIE (1<<1)




static bool did_init = 0;



void xiaohandelay() {
	volatile unsigned int j;

	for (j = 0; j < (10000); j++) ;	// delay 
}


void pic_enable(unsigned int irq)
{
	assert(irq < 8);
	uint32_t sr = read_c0_status();
	sr |= 1 << (irq + STATUSB_IP0);
	sr  = sr | 0x0000fc00;//added by xiaohan. For debug and practical use. Enable all interrupt.
	write_c0_status(sr);
}

void pic_disable(unsigned int irq)
{
	assert(irq < 8);
	uint32_t sr = read_c0_status();
	sr &= ~(1 << (irq + STATUSB_IP0));
	write_c0_status(sr);
}

void pic_init(void)
{
	uint32_t sr = read_c0_status();
	/* disable all */
	sr &= ~ST0_IM;
	write_c0_status(sr);
	did_init = 1;
	xilinx_intc_init();//this is initializing the External Interrupt Controller!
					   //this function is available in picirq.h for simplicity.
}


void xilinx_intc_init()
{
	/*
	 * Disable all external interrupts until they are
	 * explicity requested.
	 */
	*WRITE_IO(INTC_BASE + INTC_IER) = 0x00000000; 
	xiaohandelay( );

	/* Acknowledge any pending interrupts just in case. */
	*WRITE_IO(INTC_BASE + INTC_IAR) = 0xffffffff;
	xiaohandelay( );

	/* Turn on the Master Enable. */
	*WRITE_IO(INTC_BASE + INTC_MER) = INTC_MER_HIE | INTC_MER_ME;
	xiaohandelay( );
	
	if (!(*READ_IO(INTC_BASE + INTC_MER) & (INTC_MER_HIE | INTC_MER_ME))) {
		//uart_print("The MER is not enabled!\n\r");
		*WRITE_IO(INTC_BASE + INTC_MER) = INTC_MER_HIE | INTC_MER_ME;
	}
	
	/*
	 * Enable INT0-INT7 external interrupts
	 */
	*WRITE_IO(INTC_BASE + INTC_IER) = 0x000000ff; 
	
	//uart_print("The AXI4 interrupt controller is initialized!\n\r");
	
	return;
}