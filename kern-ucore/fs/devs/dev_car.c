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

void writeCar(int fl,int fr,int bl,int br,int spi){
		*WRITE_IO(CAR_DIR) = spi;
		delay();
		*WRITE_IO(PWM_F_L_BASE) = fl*100000;  
		delay();  
		*WRITE_IO(PWM_F_R_BASE) = fr*100000; 
		delay();   
		*WRITE_IO(PWM_B_L_BASE) = bl*100000; 
		delay();   
		*WRITE_IO(PWM_B_R_BASE) = br*100000;
		delay();
}

static int car_open(struct device *dev, uint32_t open_flags)
{
	if (open_flags != O_WRONLY && open_flags != O_RDONLY) {
		return -E_INVAL;
	}
	return 0;
}

static int car_close(struct device *dev)
{
	return 0;
}

static int car_io(struct device *dev, struct iobuf *iob, bool write)
{
	int spd, stat;
	if (write) {
			char *data = iob->io_base;
			memcpy(&spd, data, 4);
			memcpy(&stat, data+4, 4);
			kprintf("spd 锛?d, stat : %d",spd, stat);
			writeCar(spd, spd, spd, spd, stat);
	}
	return 0;
}

static int car_ioctl(struct device *dev, int op, void *data)
{
	return -E_INVAL;
}

static void car_device_init(struct device *dev)
{
	memset(dev, 0, sizeof(*dev));
	dev->d_blocks = 0;
	dev->d_blocksize = 1;
	dev->d_open = car_open;
	dev->d_close = car_close;
	dev->d_io = car_io;
	dev->d_ioctl = car_ioctl;
}

void dev_init_car(void)
{
	struct inode *node;
	if ((node = dev_create_inode()) == NULL) {
		panic("car: dev_create_node.\n");
	}
	car_device_init(vop_info(node, device));

	int ret;
  struct dev_index index = vfs_register_dev(6, "car");
  if (dev_index_is_invalid(index)) {
    panic("car: vfs_register_dev error.\n");
  }
	if ((ret = vfs_add_dev(index, "car", node, 0)) != 0) {
		panic("car: vfs_add_dev: %e.\n", ret);
	}
}

void dev_init_sfs_inode_car(void) {
  int ret;
  struct dev_index index = vfs_get_dev_index("car");
  if ((ret = dev_make_sfs_inode("car", index)) != 0) {
    panic("car: dev_make_sfs_inode: %e.\n", ret);
  }
}
