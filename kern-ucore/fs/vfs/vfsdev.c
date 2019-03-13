/*
 * VFS operations that involve the list of VFS (named) devices
 * (the "dev" in "dev:path" syntax).
 */
#include <types.h>
#include <stdio.h>
#include <string.h>
#include <vfs.h>
#include <dev.h>
#include <inode.h>
#include <sem.h>
#include <list.h>
#include <pmm.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>
#include <mips_io.h>

/*
 * Structure for a single named device.
 * 
 * devname    - Name of device (eg, "lhd0"). Should always be set to
 *              a valid string.
 *
 * devnode    - inode of device .
 *
 * fs         - Filesystem object mounted on, or associated with, this
 *              device. NULL if there is no filesystem. 
 *
 * A filesystem can be associated with a device without having been
 * mounted if the device was created that way. 
 * Referencing devname, or the filesystem volume name, on a device
 * with a filesystem mounted returns the root of the filesystem.
 * Referencing devname on a mountable device with no filesystem
 * returns -E_NA_DEV. Referencing devname on a device that is not
 * mountable and has no filesystem, returns the device itself.
 */
typedef struct {
	const char *devname;
  unsigned int minorindex;
	struct inode *devnode;
	struct fs *fs;
	bool mountable;
	list_entry_t vdev_link;
} vfs_dev_t;

#define le2vdev(le, member)                         \
    to_struct((le), vfs_dev_t, member)

typedef struct {
  unsigned int index;
  list_entry_t vdev_list;
  list_entry_t vdev_major_link;
} vfs_dev_major;

#define le2vdev_major(le, member)                  \
    to_struct((le), vfs_dev_major, member)

static list_entry_t vdev_major_list;
static semaphore_t vdev_major_list_sem;

static void lock_vdev_major_list(void)
{
	down(&vdev_major_list_sem);
}

static void unlock_vdev_major_list(void)
{
	up(&vdev_major_list_sem);
}

void vfs_devlist_init(void)
{
	list_init(&vdev_major_list);
	sem_init(&vdev_major_list_sem, 1);
}

/*
 * clean up all FS
 */
void vfs_cleanup(void)
{
  if (!list_empty(&vdev_major_list)) {
    lock_vdev_major_list();
    {
      list_entry_t *list = &vdev_major_list, *le = list;
      while ((le = list_next(le)) != list) {
        vfs_dev_major *vdev_major = le2vdev_major(le, vdev_major_link);
        if (!list_empty(&(vdev_major->vdev_list))) {
            list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
            while ((devle = list_next(devle)) != devlist) {
              vfs_dev_t *vdev = le2vdev(devle, vdev_link);
              if (vdev->fs != NULL) {
                fsop_cleanup(vdev->fs);
              }
            }
        }
      }
    }
    unlock_vdev_major_list();
  }
}

/*
 * Global sync function - call fsos_sync on all devices.
 */
int vfs_sync(void)
{
  if (!list_empty(&vdev_major_list)) {
    lock_vdev_major_list();
    {
      list_entry_t *list = &vdev_major_list, *le = list;
      while ((le = list_next(le)) != list) {
        vfs_dev_major *vdev_major = le2vdev_major(le, vdev_major_link);
        if (!list_empty(&(vdev_major->vdev_list))) {
          list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
          while ((devle = list_next(devle)) != devlist) {
            vfs_dev_t *vdev = le2vdev(devle, vdev_link);
            if (vdev->fs != NULL) {
              fsop_sync(vdev->fs);
            }
          }
        }
      }
    }
    unlock_vdev_major_list();
  }
	return 0;
}

/*
 * Given a device name (lhd0, emu0, somevolname, null, etc.), hand
 * back an appropriate inode.
 */
int vfs_get_root(const char *devname, struct inode **node_store)
{
	assert(devname != NULL);
  int ret = -E_NO_DEV;
  if (!list_empty(&vdev_major_list)) {
    lock_vdev_major_list();
    {
      list_entry_t *list = &vdev_major_list, *le = list;
      while ((le = list_next(le)) != list) {
        vfs_dev_major *vdev_major = le2vdev_major(le, vdev_major_link);
        if (!list_empty(&(vdev_major->vdev_list))) {
          list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
          while ((devle = list_next(devle)) != devlist) {
            vfs_dev_t *vdev = le2vdev(devle, vdev_link);
            if (strcmp(devname, vdev->devname) == 0) {
              struct inode *found = NULL;

              /*
               * If this device has a mounted filesystem, and
               * DEVNAME names either the filesystem or the device,
               * return the root of the filesystem.
               *
               * If it has no mounted filesystem, it's mountable,
               * and DEVNAME names the device, return -E_NA_DEV.
               */

              if (vdev->fs != NULL) {
                found = fsop_get_root(vdev->fs);
              } else if (!vdev->mountable) {
                vop_ref_inc(vdev->devnode);
                found = vdev->devnode;
              }
              if (found != NULL) {
                /*
                 * If DEVNAME names the device, and we get here, it
                 * must have no fs and not be mountable. In this case,
                 * we return the inode of device itself--node_store.
                 */
                ret = 0, *node_store = found;
              } else {
                /*
                 * If we got here, the device specified by devname doesn't exist.
                 */
                ret = -E_NA_DEV;
              }
              break;
            }
            /*
             * If none of the above tests matched, we didn't name
             * any of the names of this device, so go on to the
             * next one.
             */
          }
        }
      }
    }
    unlock_vdev_major_list();
  }
  return ret;
}

/*
 * Given a filesystem, hand back the name of the device it's mounted on.
 */
const char *vfs_get_devname(struct fs *fs)
{
	assert(fs != NULL);
  list_entry_t *list = &vdev_major_list, *le = list;
  while ((le = list_next(le)) != list) {
    vfs_dev_major *vdev_major = le2vdev_major(le, vdev_major_link);
    if (!list_empty(&(vdev_major->vdev_list))) {
      list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
      while ((devle = list_next(devle)) != devlist) {
        vfs_dev_t *vdev = le2vdev(devle, vdev_link);
        if (vdev->fs == fs) {
          /*
           * This is not a race condition: as long as the
           * guy calling us holds a reference to the fs,
           * the fs cannot go away, and the device can't
           * go away until the fs goes away.
           */
          return vdev->devname;
        }
      }
    }
  }
  return NULL;
}

static bool check_devname_conflict(const char *devname)
{
  list_entry_t *list = &vdev_major_list, *le = list;
  while ((le = list_next(le)) != list) {
    vfs_dev_major *vdev_major = le2vdev_major(le, vdev_major_link);
    if (!list_empty(&(vdev_major->vdev_list))) {
      list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
      while ((devle = list_next(devle)) != devlist) {
        vfs_dev_t *vdev = le2vdev(devle, vdev_link);
        if (strcmp(vdev->devname, devname) == 0) {
          return 0;
        }
      }
    }
  }
  return 1;
}


static vfs_dev_major *get_dev_major(unsigned int major) {
  vfs_dev_major *result = NULL;
  if (!list_empty(&vdev_major_list)) {
    list_entry_t *list = &vdev_major_list, *le = list;
    while ((le = list_next(le)) != list) {
      vfs_dev_major *vdev_major = le2vdev_major(le, vdev_major_link);
      if (vdev_major->index == major) {
        result = vdev_major;
        break;
      }
    }
  }
  return result;
}

int vfs_get_devnode(struct dev_index index, struct inode **node_store) {
  vfs_dev_major *vdev_major;
  if ((vdev_major = get_dev_major(index.major)) == NULL) {
    return -E_NO_DEV;
  }
  if (!list_empty(&(vdev_major->vdev_list))) {
    list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
    while ((devle = list_next(devle)) != devlist) {
      vfs_dev_t *vdev = le2vdev(devle, vdev_link);
      if (vdev->minorindex == index.minor) {
        struct inode *found = NULL;
        vop_ref_inc(vdev->devnode);
        found = vdev->devnode;

        if (found != NULL) {
          *node_store = found;
          return 0;
        } else {
          return -E_NA_DEV;
        }
      }
    }
  }
  return -E_NO_DEV;
}

bool check_dev_exist(struct dev_index index) {
  vfs_dev_major *vdev_major;
  if ((vdev_major = get_dev_major(index.major)) == NULL) {
    return 0;
  }
  if (!list_empty(&(vdev_major->vdev_list))) {
    list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
    while ((devle = list_next(devle)) != devlist) {
      vfs_dev_t *vdev = le2vdev(devle, vdev_link);
      if (vdev->minorindex == index.minor) {
        return 1;
      }
    }
  }
  return 0;
}

struct dev_index vfs_get_dev_index(const char *devname)
{
  struct dev_index index = INVAL_VFS_INDEX;
  list_entry_t *list = &vdev_major_list, *le = list;
  while ((le = list_next(le)) != list) {
    vfs_dev_major *vdev_major = le2vdev_major(le, vdev_major_link);
    if (!list_empty(&(vdev_major->vdev_list))) {
      list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
      while ((devle = list_next(devle)) != devlist) {
        vfs_dev_t *vdev = le2vdev(devle, vdev_link);
        if (strcmp(vdev->devname, devname) == 0) {
          index.major = vdev_major->index;
          index.minor = vdev->minorindex;
          return index;
        }
      }
    }
  }
  return index;
}

/*
 * Add a new device to the VFS layer's device table.
 *
 * If "mountable" is set, the device will be treated as one that expects
 * to have a filesystem mounted on it.
 */
static int
vfs_do_add(struct dev_index index, const char *devname, struct inode *devnode, struct fs *fs,
	   bool mountable)
{
	assert(devname != NULL);
	assert((devnode == NULL && !mountable)
	       || (devnode != NULL && check_inode_type(devnode, device)));
	if (strlen(devname) > FS_MAX_DNAME_LEN) {
		return -E_TOO_BIG;
	}

	int ret = -E_NO_MEM;
	char *s_devname;
	if ((s_devname = strdup(devname)) == NULL) {
		return ret;
	}

	vfs_dev_t *vdev;
	if ((vdev = kmalloc(sizeof(vfs_dev_t))) == NULL) {
		goto failed_cleanup_name;
	}

	ret = -E_EXISTS;
	lock_vdev_major_list();
	if (!check_devname_conflict(s_devname)) {
		unlock_vdev_major_list();
		goto failed_cleanup_vdev;
	}
	vdev->devname = s_devname;
	vdev->devnode = devnode;
	vdev->mountable = mountable;
	vdev->fs = fs;
  vdev->minorindex = index.minor;
  
  vfs_dev_major *vdev_major;
  if ((vdev_major = get_dev_major(index.major)) == NULL) {
    return -E_NO_DEV;
  }

  list_add(&(vdev_major->vdev_list), &(vdev->vdev_link));
	unlock_vdev_major_list();
	return 0;

failed_cleanup_vdev:
	kfree(vdev);
failed_cleanup_name:
	kfree(s_devname);
	return ret;
}

/*
 * Add a filesystem that does not have an underlying device.
 * This is used for emufs(not implementation???), but might also be used for network
 * filesystems and the like.
 */
int vfs_add_fs(struct dev_index index, const char *devname, struct fs *fs)
{
	return vfs_do_add(index, devname, NULL, fs, 0);
}

/*
 * Add a new device, by name. See above for the description of
 * mountable.
 */
int vfs_add_dev(struct dev_index index, const char *devname, struct inode *devnode, bool mountable)
{
	return vfs_do_add(index, devname, devnode, NULL, mountable);
}

/*
 * Register a new device before adding.
 */
struct dev_index vfs_register_dev(unsigned int major, const char *devname) {
  vfs_dev_major *vdev_major = get_dev_major(major);
  struct dev_index index = INVAL_VFS_INDEX;
  if (vdev_major == NULL) {
    if ((vdev_major = kmalloc(sizeof(vfs_dev_major))) == NULL) {
      return index;
    }
    vdev_major->index = major;
    list_init(&vdev_major->vdev_list);
    lock_vdev_major_list();
    list_add(&(vdev_major_list), &(vdev_major->vdev_major_link));
    unlock_vdev_major_list();
  }
  index.major = vdev_major->index;
  index.minor = 0;
  list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
  while ((devle = list_next(devle)) != devlist) {
    ++index.minor;
  }
  return index;
}

int dev_getdevinfo(struct devinfo *cur_dev, struct devinfo *next_dev)
{
  if (cur_dev == NULL) {
    list_entry_t *le = list_next(&vdev_major_list);
    if (le != &vdev_major_list) {
      vfs_dev_major *vdev_major = le2vdev_major(le, vdev_major_link);
      list_entry_t *devle = list_next(&(vdev_major->vdev_list));
      if (devle != &(vdev_major->vdev_list)) {
        vfs_dev_t *vdev = le2vdev(devle, vdev_link);
        next_dev->major = vdev_major->index;
        next_dev->minor = vdev->minorindex;
        strcpy(next_dev->name, vdev->devname);
        return 0;
      }
    }
  } else {
    vfs_dev_major *vdev_major = get_dev_major(cur_dev->major);
    if (vdev_major != NULL) {
      list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
      while ((devle = list_next(devle)) != devlist) {
        vfs_dev_t *vdev = le2vdev(devle, vdev_link);
        if (vdev->minorindex == cur_dev->minor) {
          if (list_next(devle) != devlist) {
            vdev = le2vdev(list_next(devle), vdev_link);
            next_dev->major = vdev_major->index;
            next_dev->minor = vdev->minorindex;
            strcpy(next_dev->name, vdev->devname);
            return 0;
          } else {
            list_entry_t *major_nextle = list_next(&(vdev_major->vdev_major_link));
            if (major_nextle != &vdev_major_list) {
              vdev_major = le2vdev_major(major_nextle, vdev_major_link);
              list_entry_t *devle = list_next(&(vdev_major->vdev_list));
              if (devle != &(vdev_major->vdev_list)) {
                vfs_dev_t *vdev = le2vdev(devle, vdev_link);
                next_dev->major = vdev_major->index;
                next_dev->minor = vdev->minorindex;
                strcpy(next_dev->name, vdev->devname);
                return 0;
              }
            }
          }
        }
      }
    }
  }
  return -E_NO_DEV;
}


/*
 * Look for a mountable device named DEVNAME.
 * Should already hold an lock::vdev_list_sem.
 */
static int find_mount(const char *devname, vfs_dev_t ** vdev_store)
{
	assert(devname != NULL);
  list_entry_t *list = &vdev_major_list, *le = list;
  while ((le = list_next(le)) != list) {
    vfs_dev_major *vdev_major = le2vdev_major(le, vdev_major_link);
    if (!list_empty(&(vdev_major->vdev_list))) {
      list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
      while ((devle = list_next(devle)) != devlist) {
        vfs_dev_t *vdev = le2vdev(devle, vdev_link);
        if (vdev->mountable && strcmp(vdev->devname, devname) == 0) {
          *vdev_store = vdev;
          return 0;
        }
      }
    }
  }
  return -E_NO_DEV;
}

/*
 * Mount a filesystem. Once we've found the device, call MOUNTFUNC to
 * set up the filesystem and hand back a struct fs.
 */
int
vfs_mount(const char *devname,
	  int (*mountfunc) (struct device * dev, struct fs ** fs_store))
{
	int ret;
	lock_vdev_major_list();
	vfs_dev_t *vdev;
	if ((ret = find_mount(devname, &vdev)) != 0) {
		goto out;
	}
	if (vdev->fs != NULL) {
		ret = -E_BUSY;
		goto out;
	}
	assert(vdev->devname != NULL && vdev->mountable);

	struct device *dev = vop_info(vdev->devnode, device);
	if ((ret = mountfunc(dev, &(vdev->fs))) == 0) {
		assert(vdev->fs != NULL);
		kprintf("\n\rvfs: mount %s.\n\r", vdev->devname);
	}

out:
	unlock_vdev_major_list();
	return ret;
}

/*
 * Unmount a filesystem/device by name.
 * First calls fsop_sync on the filesystem; then calls fsop_umount.
 */
int vfs_unmount(const char *devname)
{
	int ret;
	lock_vdev_major_list();
	vfs_dev_t *vdev;
	if ((ret = find_mount(devname, &vdev)) != 0) {
		goto out;
	}
	if (vdev->fs == NULL) {
		ret = -E_INVAL;
		goto out;
	}
	assert(vdev->devname != NULL && vdev->mountable);

	if ((ret = fsop_sync(vdev->fs)) != 0) {
		goto out;
	}
	if ((ret = fsop_unmount(vdev->fs)) == 0) {
		/* now drop the filesystem */
		vdev->fs = NULL;
		kprintf("vfs: unmount %s.\n\r", vdev->devname);
	}

out:
	unlock_vdev_major_list();
	return ret;
}

/*
 * Global unmount function.
 */
int vfs_unmount_all(void)
{
  if (!list_empty(&vdev_major_list)) {
    lock_vdev_major_list();
    {
      list_entry_t *list = &vdev_major_list, *le = list;
      while ((le = list_next(le)) != list) {
        vfs_dev_major *vdev_major = le2vdev_major(le, vdev_major_link);
        if (!list_empty(&(vdev_major->vdev_list))) {
          list_entry_t *devlist = &(vdev_major->vdev_list), *devle = devlist;
          while ((devle = list_next(devle)) != devlist) {
            vfs_dev_t *vdev = le2vdev(devle, vdev_link);
            if (vdev->mountable && vdev->fs != NULL) {
              int ret;
              if ((ret = fsop_sync(vdev->fs)) != 0) {
                kprintf
                ("vfs: warning: sync failed for %s: %e.\n\r",
                 vdev->devname, ret);
                continue;
              }
              if ((ret = fsop_unmount(vdev->fs)) != 0) {
                kprintf
                ("vfs: warning: unmount failed for %s: %e.\n\r",
                 vdev->devname, ret);
                continue;
              }
              /* now drop the filesystem */
              vdev->fs = NULL;
              kprintf("vfs: unmount %s.\n\r",
                      vdev->devname);
            }
          }
        }
      }
    }
    lock_vdev_major_list();
  }
  return 0;
}
