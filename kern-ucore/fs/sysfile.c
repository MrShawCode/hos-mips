#include <types.h>
#include <string.h>
#include <pmm.h>
#include <proc.h>
#include <vfs.h>
#include <file.h>
#include <iobuf.h>
#include <sysfile.h>
#include <stat.h>
#include <dirent.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>

#define IOBUF_SIZE                          4096

static int copy_path(char **to, const char *from)
{
	char *buffer;
	if ((buffer = kmalloc(FS_MAX_FPATH_LEN + 1)) == NULL) {
		return -E_NO_MEM;
	}
	
	if (!copy_string(buffer, from, FS_MAX_FPATH_LEN + 1)) {
		
		goto failed_cleanup;
	}
	
	*to = buffer;
	return 0;

failed_cleanup:
	kfree(buffer);
	return -E_INVAL;
}

int sysfile_open(const char *__path, uint32_t open_flags)
{
	int ret;
	char *path;
	if ((ret = copy_path(&path, __path)) != 0) {
		return ret;
	}
	ret = file_open(path, open_flags);
	kfree(path);
	return ret;
}

int sysfile_close(int fd)
{
	return file_close(fd);
}

int sysfile_read(int fd, void *base, size_t len)
{
	int ret = 0;
	if (len == 0) {
		return 0;
	}
	if (!file_testfd(fd, 1, 0)) {
		return -E_INVAL;
	}
	/* for linux inode */
	if (__is_linux_devfile(fd)) {
		size_t alen = 0;
		ret = linux_devfile_read(fd, base, len, &alen);
		if (ret)
			return ret;
		return alen;
	}
	void *buffer;
	if ((buffer = kmalloc(IOBUF_SIZE)) == NULL) {
		return -E_NO_MEM;
	}

	size_t copied = 0, alen;
	while (len != 0) {
		if ((alen = IOBUF_SIZE) > len) {
			alen = len;
		}
		ret = file_read(fd, buffer, alen, &alen);
		if (alen != 0) {
			
			{
				if (memcpy(base, buffer, alen)) {
					assert(len >= alen);
					base += alen, len -= alen, copied +=
					    alen;
				} else if (ret == 0) {
					ret = -E_INVAL;
				}
			}
			
		}
		if (ret != 0 || alen == 0) {
			goto out;
		}
	}

out:
	kfree(buffer);
	if (copied != 0) {
		return copied;
	}
	return ret;
}

int sysfile_write(int fd, void *base, size_t len)
{
	int ret = 0;
	if (len == 0) {
		return 0;
	}
	if (!file_testfd(fd, 0, 1)) {
		return -E_INVAL;
	}
	/* for linux inode */
	if (__is_linux_devfile(fd)) {
		/* use 8byte int, in case of 64bit off_t
		 * config in linux kernel */
		size_t alen = 0;
		ret = linux_devfile_write(fd, base, len, &alen);
		if (ret)
			return ret;
		return alen;
	}
	void *buffer;
	if ((buffer = kmalloc(IOBUF_SIZE)) == NULL) {
		return -E_NO_MEM;
	}

	size_t copied = 0, alen;
	while (len != 0) {
		if ((alen = IOBUF_SIZE) > len) {
			alen = len;
		}
		
		if (!memcpy(buffer, base, alen)) {
			ret = -E_INVAL;
		}
		
		if (ret == 0) {
			ret = file_write(fd, buffer, alen, &alen);
			if (alen != 0) {
				assert(len >= alen);
				base += alen, len -= alen, copied += alen;
			}
		}
		if (ret != 0 || alen == 0) {
			goto out;
		}
	}

out:
	kfree(buffer);
	if (copied != 0) {
		return copied;
	}
	return ret;
}

int sysfile_writev(int fd, struct iovec __user * iov, int iovcnt)
{
	/* do nothing but return 0 */
	kprintf("writev: fd=%08x iov=%08x iovcnt=%d\n\r", fd, iov, iovcnt);
	struct iovec *tv;
	int rcode = 0, count = 0, i;
	for (i = 0; i < iovcnt; ++i) {
		char *pbase;
		size_t plen;

		memcpy(&pbase, &(iov[i].iov_base), sizeof(char *));
		memcpy(&plen, &(iov[i].iov_len), sizeof(size_t));

		rcode = sysfile_write(fd, pbase, plen);
		if (rcode < 0)
			break;
		count += rcode;
	}
	if (count == 0)
		return (rcode);
	else
		return (count);
}

int sysfile_seek(int fd, off_t pos, int whence)
{
	return file_seek(fd, pos, whence);
}

int sysfile_fstat(int fd, struct stat *__stat)
{
	int ret;
	struct stat __local_stat, *stat = &__local_stat;
	if ((ret = file_fstat(fd, stat)) != 0) {
		return ret;
	}

	if (!memcpy(__stat, stat, sizeof(struct stat))) {
		ret = -E_INVAL;
	}

	return ret;
}

int sysfile_linux_fstat(int fd, struct linux_stat __user * buf)
{
	int ret;
	struct stat __local_stat, *kstat = &__local_stat;
	if ((ret = file_fstat(fd, kstat)) != 0) {
		return -1;
	}
	struct linux_stat *kls = kmalloc(sizeof(struct linux_stat));
	if (!kls) {
		return -1;
	}
	memset(kls, 0, sizeof(struct linux_stat));
	kls->st_ino = 1;
	/* ucore never check access permision */
	kls->st_mode = kstat->st_mode | 0777;
	kls->st_nlink = kstat->st_nlinks;
	kls->st_blksize = 512;
	kls->st_blocks = kstat->st_blocks;
	kls->st_size = kstat->st_size;

	ret = 0;
	
	{
		if (!memcpy(buf, kls, sizeof(struct linux_stat))) {
			ret = -1;
		}
	}
	
	kfree(kls);
	return ret;
}

int sysfile_linux_fstat64(int fd, struct linux_stat64 __user * buf)
{
	int ret;
	struct stat __local_stat, *kstat = &__local_stat;
	if ((ret = file_fstat(fd, kstat)) != 0) {
		return -1;
	}
	struct linux_stat64 *kls = kmalloc(sizeof(struct linux_stat64));
	if (!kls) {
		return -1;
	}
	memset(kls, 0, sizeof(struct linux_stat64));
	kls->st_ino = 1;
	/* ucore never check access permision */
	kls->st_mode = kstat->st_mode | 0777;
	kls->st_nlink = kstat->st_nlinks;
	kls->st_blksize = 512;
	kls->st_blocks = kstat->st_blocks;
	kls->st_size = kstat->st_size;

	ret = 0;
	
	{
		if (!memcpy(buf, kls, sizeof(struct linux_stat64))) {
			ret = -1;
		}
	}
	
	kfree(kls);
	return ret;
}

int sysfile_linux_fcntl64(int fd, int cmd, int arg)
{
	kprintf("sysfile_linux_fcntl64:fd=%08x cmd=%08x arg=%08x\n\r", fd, cmd,
		arg);
	return 0;
}

int sysfile_linux_stat(const char __user * fn, struct linux_stat *__user buf)
{
	int fd = sysfile_open(fn, O_RDONLY);
	if (fd < 0)
		return -1;
	int ret = sysfile_linux_fstat(fd, buf);
	sysfile_close(fd);

	return ret;
}

int
sysfile_linux_stat64(const char __user * fn, struct linux_stat64 *__user buf)
{
	int fd = sysfile_open(fn, O_RDONLY);
	if (fd < 0)
		return -1;
	int ret = sysfile_linux_fstat64(fd, buf);
	sysfile_close(fd);

	return ret;
}

int sysfile_fsync(int fd)
{
	return file_fsync(fd);
}

int sysfile_chdir(const char *__path)
{
	int ret;
	char *path;
	if ((ret = copy_path(&path, __path)) != 0) {
		return ret;
	}
	ret = vfs_chdir(path);
	kfree(path);
	return ret;
}

int sysfile_mkdir(const char *__path)
{
	int ret;
	char *path;
	if ((ret = copy_path(&path, __path)) != 0) {
		return ret;
	}
	ret = vfs_mkdir(path);
	kfree(path);
	return ret;
}

int sysfile_link(const char *__path1, const char *__path2)
{
	int ret;
	char *old_path, *new_path;
	if ((ret = copy_path(&old_path, __path1)) != 0) {
		return ret;
	}
	if ((ret = copy_path(&new_path, __path2)) != 0) {
		kfree(old_path);
		return ret;
	}
	ret = vfs_link(old_path, new_path);
	kfree(old_path), kfree(new_path);
	return ret;
}

int sysfile_rename(const char *__path1, const char *__path2)
{
	int ret;
	char *old_path, *new_path;
	if ((ret = copy_path(&old_path, __path1)) != 0) {
		return ret;
	}
	if ((ret = copy_path(&new_path, __path2)) != 0) {
		kfree(old_path);
		return ret;
	}
	ret = vfs_rename(old_path, new_path);
	kfree(old_path), kfree(new_path);
	return ret;
}

int sysfile_unlink(const char *__path)
{
	int ret;
	char *path;
	if ((ret = copy_path(&path, __path)) != 0) {
		return ret;
	}
	ret = vfs_unlink(path);
	kfree(path);
	return ret;
}

int sysfile_getcwd(char *buf, size_t len)
{
	if (len == 0) {
		return -E_INVAL;
	}

	struct iobuf __iob, *iob =
		iobuf_init(&__iob, buf, len, 0);
	vfs_getcwd(iob);

	return 0;
}

int sysfile_getdirentry(int fd, struct dirent *__direntp, uint32_t * len_store)
{
	struct dirent *direntp;
	if ((direntp = kmalloc(sizeof(struct dirent))) == NULL) {
		return -E_NO_MEM;
	}

	int ret = 0;
	
	if (!memcpy(&(direntp->d_off), &(__direntp->d_off),
			sizeof(direntp->d_off))) {
		ret = -E_INVAL;
	}
	

	if (ret != 0 || (ret = file_getdirentry(fd, direntp)) != 0) {
		goto out;
	}

	if (!memcpy(__direntp, direntp, sizeof(struct dirent))) {
		ret = -E_INVAL;
	}
	
	if (len_store) {
		*len_store = (direntp->d_name[0]) ? direntp->d_reclen : 0;
	}
out:
	kfree(direntp);
	return ret;
}

int sysfile_dup(int fd1, int fd2)
{
	return file_dup(fd1, fd2);
}

int sysfile_pipe(int *fd_store)
{
	int ret, fd[2];
	if ((ret = file_pipe(fd)) == 0) {
		
		{
			if (!memcpy(fd_store, fd, sizeof(fd))) {
				ret = -E_INVAL;
			}
		}
		
		if (ret != 0) {
			file_close(fd[0]), file_close(fd[1]);
		}
	}
	return ret;
}

int sysfile_mkfifo(const char *__name, uint32_t open_flags)
{
	int ret;
	char *name;
	if ((ret = copy_path(&name, __name)) != 0) {
		return ret;
	}
	ret = file_mkfifo(name, open_flags);
	kfree(name);
	return ret;
}

int sysfile_ioctl(int fd, unsigned int cmd, unsigned long arg)
{
	if (!file_testfd(fd, 1, 0)) {
		return -E_INVAL;
	}
	if (!__is_linux_devfile(fd)) {
		return -E_INVAL;
	}
	return linux_devfile_ioctl(fd, cmd, arg);
}
