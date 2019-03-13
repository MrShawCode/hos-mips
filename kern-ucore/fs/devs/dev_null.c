/*
 * Implementation of the null device, "null:", which generates an
 * immediate EOF on read and throws away anything written to it.
 */
#include <types.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <inode.h>
#include <error.h>
#include <assert.h>

/* For open() */
static int null_open(struct device *dev, uint32_t open_flags)
{
	return 0;
}

/* For close() */
static int null_close(struct device *dev)
{
	return 0;
}

/* For dop_io() */
static int null_io(struct device *dev, struct iobuf *iob, bool write)
{
/*
 * On write, discard everything without looking at it.
 * On read, do nothing, generating an immediate EOF.
 */
	if (write) {
		iob->io_resid = 0;
	}
	return 0;
}

/* For ioctl() */
static int null_ioctl(struct device *dev, int op, void *data)
{
	return -E_INVAL;
}

static void null_device_init(struct device *dev)
{
	memset(dev, 0, sizeof(*dev));
	dev->d_blocks = 0;
	dev->d_blocksize = 1;
	dev->d_open = null_open;
	dev->d_close = null_close;
	dev->d_io = null_io;
	dev->d_ioctl = null_ioctl;
}

/*
 * Function to create and attach null:
 */
void dev_init_null(void)
{
	struct inode *node;
	if ((node = dev_create_inode()) == NULL) {
		panic("null: dev_create_node.\n");
	}
	null_device_init(vop_info(node, device));

	int ret;
  struct dev_index index = vfs_register_dev(0, "null");
  if (dev_index_is_invalid(index)) {
    panic("null: vfs_register_dev error.\n");
  }
	if ((ret = vfs_add_dev(index, "null", node, 0)) != 0) {
		panic("null: vfs_add_dev: %e.\n", ret);
	}
}

void dev_init_sfs_inode_null(void) {
  int ret;
  struct dev_index index = vfs_get_dev_index("null");
  if ((ret = dev_make_sfs_inode("null", index)) != 0) {
    panic("null: dev_make_sfs_inode: %e.\n", ret);
  }
}
