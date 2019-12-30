#include <types.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>
#include <mboxbuf.h>
#include <stat.h>
#include <dirent.h>
#include <signal.h>
#include <defs.h>

#define MAX_ARGS            5
#define SYSCALL_BASE        0x80

uintptr_t syscall(int num, ...)
{
	va_list ap;
	va_start(ap, num);
	uint32_t arg[MAX_ARGS];
	int i, ret;
	for (i = 0; i < MAX_ARGS; i++) {
		arg[i] = va_arg(ap, uint32_t);
	}
	va_end(ap);

	//num += SYSCALL_BASE;//modified
	asm volatile (".set noreorder;\n" "move $v0, %1;\n"	/* syscall no. */
		      "move $a0, %2;\n"
		      "move $a1, %3;\n"
		      "move $a2, %4;\n"
		      "move $a3, %5;\n"
		      "move $t0, %6;\n"
		      "syscall;\n" "nop;\n" "move %0, $v0;\n":"=r" (ret)
		      :"r"(num), "r"(arg[0]), "r"(arg[1]), "r"(arg[2]),
		      "r"(arg[3]), "r"(arg[4])
		      :"a0", "a1", "a2", "a3", "v0", "t0");
	return ret + 1 - 1;
}

int sys_exit(int error_code)
{
	return syscall(SYS_exit, error_code);
}

int sys_fork(void)
{
	return syscall(SYS_fork);
}

int sys_wait(int pid, int *store)
{
	return syscall(SYS_wait, pid, store);
}

int sys_exec(const char *filename, const char **argv, const char **envp)
{
	return syscall(SYS_exec, filename, argv, envp);
}

int sys_yield(void)
{
	return syscall(SYS_yield);
}

int sys_sleep(unsigned int time)
{
	return syscall(SYS_sleep, time);
}

int sys_kill(int pid)
{
	return syscall(SYS_kill, pid);
}

size_t sys_gettime(void)
{
	return (size_t) syscall(SYS_gettime);
}

int sys_getpid(void)
{
	return syscall(SYS_getpid);
}

int sys_putc(int c)
{
	return syscall(SYS_putc, c);
}

int sys_pgdir(void)
{
	return syscall(SYS_pgdir);
}

sem_t sys_sem_init(int value)
{
	return syscall(SYS_sem_init, value);
}

int sys_sem_post(sem_t sem_id)
{
	return syscall(SYS_sem_post, sem_id);
}

int sys_sem_wait(sem_t sem_id, unsigned int timeout)
{
	return syscall(SYS_sem_wait, sem_id, timeout);
}

int sys_sem_free(sem_t sem_id)
{
	return syscall(SYS_sem_free, sem_id);
}

int sys_sem_get_value(sem_t sem_id, int *value_store)
{
	return syscall(SYS_sem_get_value, sem_id, value_store);
}

int sys_send_event(int pid, int event, unsigned int timeout)
{
	return syscall(SYS_event_send, pid, event, timeout);
}

int sys_recv_event(int *pid_store, int *event_store, unsigned int timeout)
{
	return syscall(SYS_event_recv, pid_store, event_store, timeout);
}

int sys_mbox_init(unsigned int max_slots)
{
	return syscall(SYS_mbox_init, max_slots);
}

int sys_mbox_send(int id, struct mboxbuf *buf, unsigned int timeout)
{
	return syscall(SYS_mbox_send, id, buf, timeout);
}

int sys_mbox_recv(int id, struct mboxbuf *buf, unsigned int timeout)
{
	return syscall(SYS_mbox_recv, id, buf, timeout);
}

int sys_mbox_free(int id)
{
	return syscall(SYS_mbox_free, id);
}

int sys_mbox_info(int id, struct mboxinfo *info)
{
	return syscall(SYS_mbox_info, id, info);
}

int sys_open(const char *path, uint32_t open_flags)
{
	return syscall(SYS_open, path, open_flags);
}

int sys_close(int fd)
{
	return syscall(SYS_close, fd);
}

int sys_read(int fd, void *base, size_t len)
{
	return syscall(SYS_read, fd, base, len);
}

int sys_write(int fd, void *base, size_t len)
{
	return syscall(SYS_write, fd, base, len);
}

int sys_seek(int fd, off_t pos, int whence)
{
	return syscall(SYS_seek, fd, pos, whence);
}

int sys_fstat(int fd, struct stat *stat)
{
	return syscall(SYS_fstat, fd, stat);
}

int sys_fsync(int fd)
{
	return syscall(SYS_fsync, fd);
}

int sys_chdir(const char *path)
{
	return syscall(SYS_chdir, path);
}

int sys_getcwd(char *buffer, size_t len)
{
	return syscall(SYS_getcwd, buffer, len);
}

int sys_mkdir(const char *path)
{
	return syscall(SYS_mkdir, path);
}

int sys_link(const char *path1, const char *path2)
{
	return syscall(SYS_link, path1, path2);
}

int sys_rename(const char *path1, const char *path2)
{
	return syscall(SYS_rename, path1, path2);
}

int sys_unlink(const char *path)
{
	return syscall(SYS_unlink, path);
}

int sys_getdirentry(int fd, struct dirent *dirent)
{
	return syscall(SYS_getdirentry, fd, dirent);
}

int sys_dup(int fd1, int fd2)
{
	return syscall(SYS_dup, fd1, fd2);
}

int sys_pipe(int *fd_store)
{
	return syscall(SYS_pipe, fd_store);
}

int sys_mkfifo(const char *name, uint32_t open_flags)
{
	return syscall(SYS_mkfifo, name, open_flags);
}

int
sys_mount(const char *source, const char *target, const char *filesystemtype,
	  const void *data)
{
	return syscall(SYS_mount, source, target, filesystemtype, data);
}

int sys_umount(const char *target)
{
	return syscall(SYS_umount, target);
}

int sys_mknod(const char *path, unsigned major, unsigned minor) {
  return syscall(SYS_mknod, path, major, minor);
}

int sys_getdevinfo(struct devinfo *cur_dev, struct devinfo *next_dev) {
  return syscall(SYS_getdevinfo, cur_dev, next_dev);
}
