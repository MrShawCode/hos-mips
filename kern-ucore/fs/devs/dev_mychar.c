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

#define CHAR_BUFSIZE 100

static char *mychar_buffer;
static semaphore_t mychar_sem;

static void lock_mychar(void)
{
	down(&(mychar_sem));
}

static void unlock_mychar(void)
{
	up(&(mychar_sem));
}

static int mychar_open(struct device *dev, uint32_t open_flags)
{
	if (open_flags != O_RDWR && open_flags != O_RDONLY) {
		return -E_INVAL;
	}
	return 0;
}

static int mychar_close(struct device *dev)
{
	return 0;
}

static int mychar_io(struct device *dev, struct iobuf *iob, bool write)
{
	off_t offset = iob->io_offset;
	size_t resid = iob->io_resid;
	lock_mychar();
	while (resid != 0) {
		size_t copied, alen = CHAR_BUFSIZE;
		if (write) {
			iobuf_move(iob, mychar_buffer, alen, 0, &copied);
			assert(copied != 0 && copied <= resid);
		}
		else {
			// kprintf("read mychar_buffer : %s \r\n", mychar_buffer);
			if (alen > resid) {
				alen = resid;
			}
			iobuf_move(iob, mychar_buffer, alen, 1, &copied);
		}
		resid -= copied;
	}	
	unlock_mychar();
	return 0;
}

static int mychar_ioctl(struct device *dev, int op, void *data)
{
	return -E_INVAL;
}

static void mychar_device_init(struct device *dev)
{
	memset(dev, 0, sizeof(*dev));
	dev->d_blocks = 0;
	dev->d_blocksize = 1;
	dev->d_open = mychar_open;
	dev->d_close = mychar_close;
	dev->d_io = mychar_io;
	dev->d_ioctl = mychar_ioctl;
	sem_init(&(mychar_sem), 1);
	if ((mychar_buffer = kmalloc(CHAR_BUFSIZE)) == NULL) {
		panic("mychar alloc buffer failed.\n");
	}
}

void dev_init_mychar(void)
{
	struct inode *node;
	if ((node = dev_create_inode()) == NULL) {
		panic("mychar: dev_create_node.\n");
	}
	mychar_device_init(vop_info(node, device));

	int ret;
    struct dev_index index = vfs_register_dev(0, "mychar");
    if (dev_index_is_invalid(index)) {
        panic("mychar: vfs_register_dev error.\n");
    }
	if ((ret = vfs_add_dev(index, "mychar", node, 0)) != 0) {
		panic("mychar: vfs_add_dev: %e.\n", ret);
  }
}

void dev_init_sfs_inode_mychar(void) {
  int ret;
  struct dev_index index = vfs_get_dev_index("mychar");
  if ((ret = dev_make_sfs_inode("mychar", index)) != 0) {
    panic("mychar: dev_make_sfs_inode: %e.\n", ret);
  }
}
