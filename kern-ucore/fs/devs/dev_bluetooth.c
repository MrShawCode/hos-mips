#include <types.h>
#include <stdio.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>
#include <kio.h>
#include <arch.h>

#define BT_BUFSIZE 100

static char *bluetooth_buffer;

// void delay(void)
// {
//     volatile unsigned int j;
//     for (j = 0; j < (500); j++) ; // delay
// }

void init_bluetooth(void) {
    *WRITE_IO(BT_UART_BASE + lcr) = 0x00000080; // LCR[7] is 1
    delay();
    *WRITE_IO(BT_UART_BASE + dll) = 69; // DLL msb. 115200 at 50MHz. Formula is Clk/16/baudrate. From axi_uart manual.
    delay();
    *WRITE_IO(BT_UART_BASE + dlm) = 0x00000001; // DLL lsb.
    delay();
    *WRITE_IO(BT_UART_BASE + lcr) = 0x00000003; // LCR register. 8n1 parity disabled
    delay();
    *WRITE_IO(BT_UART_BASE + ier) = 0x00000001; // IER register. disable interrupts
    delay();
}

char BT_uart_inbyte(void) {
    unsigned int RecievedByte;
    while((*READ_IO(BT_UART_BASE + lsr) & 0x00000001) != 0x00000001){
        delay();
    }
    RecievedByte = *READ_IO(BT_UART_BASE + rbr);
    return (char)RecievedByte;
}

static int bluetooth_open(struct device *dev, uint32_t open_flags)
{
	if (open_flags != O_RDONLY) {
		return -E_INVAL;
    }
	return 0;
}

static int bluetooth_close(struct device *dev)
{
	return 0;
}

static int bluetooth_io(struct device *dev, struct iobuf *iob, bool write)
{
    size_t resid = iob->io_resid;
    if (!write) {
        char bt_rcvdata = BT_uart_inbyte();
        kprintf("read from bluetooth : %c \r\n", bt_rcvdata);
        bluetooth_buffer[0] = bt_rcvdata;
        kprintf("read from bluetooth_buffer : %s \r\n", bluetooth_buffer);
        size_t copied, alen = BT_BUFSIZE;
        if (alen > resid) {
            alen = resid;
        }
        iobuf_move(iob, bluetooth_buffer, alen, 1, &copied);
        // resid -= copied;
    }
	return 0;
}

static int bluetooth_ioctl(struct device *dev, int op, void *data)
{
	return -E_UNIMP;
}

static void bluetooth_device_init(struct device *dev)
{
	memset(dev, 0, sizeof(*dev));
	dev->d_blocks = 0;
	dev->d_blocksize = 1;
	dev->d_open = bluetooth_open;
	dev->d_close = bluetooth_close;
	dev->d_io = bluetooth_io;
    dev->d_ioctl = bluetooth_ioctl;
    init_bluetooth();
	if ((bluetooth_buffer = kmalloc(BT_BUFSIZE)) == NULL) {
		panic("bluetooth alloc buffer failed.\n");
	}
}

void dev_init_bluetooth(void)
{
	struct inode *node;
	if ((node = dev_create_inode()) == NULL) {
		panic("bluetooth: dev_create_node.\n");
	}
	bluetooth_device_init(vop_info(node, device));

	int ret;
    struct dev_index index = vfs_register_dev(5, "bluetooth");
    if (dev_index_is_invalid(index)) {
        panic("bluetooth: vfs_register_dev error.\n");
    }
	if ((ret = vfs_add_dev(index, "bluetooth", node, 0)) != 0) {
		panic("bluetooth: vfs_add_dev: %e.\n", ret);
  }
}

void dev_init_sfs_inode_bluetooth(void) {
  int ret;
  struct dev_index index = vfs_get_dev_index("bluetooth");
    if ((ret = dev_make_sfs_inode("bluetooth", index)) != 0) {
    panic("bluetooth: dev_make_sfs_inode: %e.\n", ret);
  }
}
