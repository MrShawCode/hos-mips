#include <types.h>
#include <syscall.h>
#include <stdio.h>
#include <stat.h>
#include <error.h>
#include <unistd.h>

int open(const char *path, uint32_t open_flags)
{
	return sys_open(path, open_flags);
}

int close(int fd)
{
	return sys_close(fd);
}

int read(int fd, void *base, size_t len)
{
	return sys_read(fd, base, len);
}

int write(int fd, void *base, size_t len)
{
	return sys_write(fd, base, len);
}

int seek(int fd, off_t pos, int whence)
{
	return sys_seek(fd, pos, whence);
}

int fstat(int fd, struct stat *stat)
{
	return sys_fstat(fd, stat);
}

int fsync(int fd)
{
	return sys_fsync(fd);
}

int dup(int fd)
{
	return sys_dup(fd, NO_FD);
}

int dup2(int fd1, int fd2)
{
	return sys_dup(fd1, fd2);
}

int pipe(int *fd_store)
{
	return sys_pipe(fd_store);
}

int mkfifo(const char *name, uint32_t open_flags)
{
	return sys_mkfifo(name, open_flags);
}

int mknod(const char *path, unsigned major, unsigned minor)
{
  return sys_mknod(path, major, minor);
}

int getdevinfo(struct devinfo *cur_dev, struct devinfo *next_dev)
{
  return sys_getdevinfo(cur_dev, next_dev);
}

static char transmode(struct stat *stat)
{
	uint32_t mode = stat->st_mode;
	if (S_ISREG(mode))
		return 'r';
	if (S_ISDIR(mode))
		return 'd';
	if (S_ISLNK(mode))
		return 'l';
	if (S_ISCHR(mode))
		return 'c';
	if (S_ISBLK(mode))
		return 'b';
	return '-';
}

void print_stat(const char *name, int fd, struct stat *stat)
{
	cprintf("[%03d] %s\n", fd, name);
	cprintf("    mode    : %c\n", transmode(stat));
	cprintf("    links   : %lu\n", stat->st_nlinks);
	cprintf("    blocks  : %lu\n", stat->st_blocks);
	cprintf("    size    : %lu\n", stat->st_size);
}
