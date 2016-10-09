#include <defs.h>
#include <arch.h>
#include <stdio.h>
#include <string.h>
#include <picirq.h>
#include <trap.h>
#include <memlayout.h>
#include <sync.h>
#include <vga.h>

/* stupid I/O delay routine necessitated by historical PC design flaws */
static void delay(void)
{
}

//#define DEBUG_COM1
/***** Serial I/O code *****/

#define COM_RX          0	// In:  Receive buffer (DLAB=0)
#define COM_TX          0	// Out: Transmit buffer (DLAB=0)
#define COM_DLL         0	// Out: Divisor Latch Low (DLAB=1)
#define COM_DLM         1	// Out: Divisor Latch High (DLAB=1)
#define COM_IER         1	// Out: Interrupt Enable Register
#define COM_IER_RDI     0x01	// Enable receiver data interrupt
#define COM_IIR         2	// In:  Interrupt ID Register
#define COM_FCR         2	// Out: FIFO Control Register
#define COM_LCR         3	// Out: Line Control Register
#define COM_LCR_DLAB    0x80	// Divisor latch access bit
#define COM_LCR_WLEN8   0x03	// Wordlength: 8 bits
#define COM_MCR         4	// Out: Modem Control Register
#define COM_MCR_RTS     0x02	// RTS complement
#define COM_MCR_DTR     0x01	// DTR complement
#define COM_MCR_OUT2    0x08	// Out2 complement
#define COM_LSR         5	// In:  Line Status Register
#define COM_LSR_DATA    0x01	// Data available
#define COM_LSR_TXRDY   0x20	// Transmit buffer avail
#define COM_LSR_TSRE    0x40	// Transmitter off

static bool serial_exists = 0;

static void serial_init(void)
{
	volatile unsigned char *uart = (unsigned char *)COM1;
	if (serial_exists)
		return;
	serial_exists = 1;
#ifdef MACH_QEMU
	// Turn off the FIFO
	outb(COM1 + COM_FCR, 0);
	// Set speed; requires DLAB latch
	outb(COM1 + COM_LCR, COM_LCR_DLAB);
	outb(COM1 + COM_DLL, (uint8_t) (115200 / 9600));
	outb(COM1 + COM_DLM, 0);

	// 8 data bits, 1 stop bit, parity off; turn off DLAB latch
	outb(COM1 + COM_LCR, COM_LCR_WLEN8 & ~COM_LCR_DLAB);

	// No modem controls
	outb(COM1 + COM_MCR, 0);
	// Enable rcv interrupts
	outb(COM1 + COM_IER, COM_IER_RDI);
#elif defined MACH_FPGA
	//TODO
#endif

	pic_enable(COM1_IRQ);
        pic_enable(KEYBOARD_IRQ);
}

static void serial_putc_sub(int c)
{
#ifdef MACH_QEMU
	outb(COM1 + COM_TX, c);
#elif defined MACH_FPGA
	//TODO
	while ((inw(COM1 + 0x04) & 0x01) == 0) ;
	outb(COM1 + 0x00, c & 0xFF);
#endif
}

/* serial_putc - print character to serial port */
static void serial_putc(int c)
{
	if (c == '\b') {
		serial_putc_sub('\b');
		serial_putc_sub(' ');
		serial_putc_sub('\b');
	} else {
		serial_putc_sub(c);
	}
}

/* serial_proc_data - get data from serial port */
static int serial_proc_data(void)
{
	int c;
#ifdef MACH_QEMU
	if (!(inb(COM1 + COM_LSR) & COM_LSR_DATA)) {
		return -1;
	}
	c = inb(COM1 + COM_RX);
#elif defined MACH_FPGA
	//TODO
	if ((inw(COM1 + 0x04) & 0x02) == 0)
		return -1;
	c = inw(COM1 + 0x00) & 0xFF;
#endif
	if (c == 127) {
		c = '\b';
	}
	return c;
}

void serial_int_handler(void *opaque)
{
#ifdef MACH_QEMU
	unsigned char id = inb(COM1 + COM_IIR);
	if (id & 0x01)
		return;
#endif
	//int c = serial_proc_data();
	int c = cons_getc();//c);
	extern void dev_stdin_write(char c);
	dev_stdin_write(c);
}

//key board handler
static const char KEYCODE_MAP[256] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,96,0,0,0,0,0,0,113,49,0,0,0,122,115,97,119,50,0,0,99,120,100,101,52,51,0,0,32,118,102,116,114,53,0,0,110,98,104,103,121,54,0,0,0,109,106,117,55,56,0,0,44,107,105,111,48,57,0,0,46,47,108,59,112,45,0,0,0,39,0,91,61,0,0,0,0,13,93,0,92,0,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,126,0,0,0,0,0,0,81,33,0,0,0,90,83,65,87,64,0,0,67,88,68,69,36,35,0,0,32,86,70,84,82,37,0,0,78,66,72,71,89,94,0,0,0,77,74,85,38,42,0,0,60,75,73,79,41,40,0,0,62,63,76,58,80,95,0,0,0,34,0,123,43,0,0,0,0,13,125,0,124,0,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

//#define DEBUG_KBD

#ifdef DEBUG_KBD
#include <stdio.h>
#endif

void keyboard_int_handler()
{
  int c = *((int*)KEYBOARD);
  if (c < 0 || c > 256)
	  return;
#ifdef DEBUG_KBD
  kprintf("input key c = %d\r\n", c);
#endif
  c = KEYCODE_MAP[c];
  if (c == 0) return;
  extern void dev_stdin_write(char c);
  dev_stdin_write(c);
}

/* *
 * Here we manage the console input buffer, where we stash characters
 * received from the keyboard or serial port whenever the corresponding
 * interrupt occurs.
 * */

#define CONSBUFSIZE 512

static struct {
	uint8_t buf[CONSBUFSIZE];
	uint32_t rpos;
	uint32_t wpos;
} cons;

/* *
 * cons_intr - called by device interrupt routines to feed input
 * characters into the circular console input buffer.
 * */
static void cons_intr(int (*proc) (void))
{
	int c;
	while ((c = (*proc) ()) != -1) {
		if (c != 0) {
			cons.buf[cons.wpos++] = c;
			if (cons.wpos == CONSBUFSIZE) {
				cons.wpos = 0;
			}
		}
	}
}

/* serial_intr - try to feed input characters from serial port */
void serial_intr(void)
{
	if (serial_exists) {
		cons_intr(serial_proc_data);
	}
}

/* cons_init - initializes the console devices */
void cons_init(void)
{
	serial_init();
#ifdef MACH_FPGA
	vga_init();
#endif
	//cons.rpos = cons.wpos = 0;
	if (!serial_exists) {
		kprintf("serial port does not exist!!\n");
	}
}

/* cons_putc - print a single character @c to console devices */
void cons_putc(int c)
{
	bool intr_flag;
	local_intr_save(intr_flag);
	{
		serial_putc(c);
#ifdef MACH_FPGA
		vga_putch(c);
#endif
	}
	local_intr_restore(intr_flag);
}

/* *
 * cons_getc - return the next input character from console,
 * or 0 if none waiting.
 * */
int cons_getc(void)
{
	int c = 0;
	bool intr_flag;
	local_intr_save(intr_flag);
	{
		// poll for any pending input characters,
		// so that this function works even when interrupts are disabled
		// (e.g., when called from the kernel monitor).
		serial_intr();

		// grab the next character from the input buffer.
		if (cons.rpos != cons.wpos) {
			c = cons.buf[cons.rpos++];
			if (cons.rpos == CONSBUFSIZE) {
				cons.rpos = 0;
			}
		}
	}
	local_intr_restore(intr_flag);
#ifdef DEBUG_COM1
	if (c) kprintf("cons_get(0x%x)\n", c);
#endif
	return c;
}
