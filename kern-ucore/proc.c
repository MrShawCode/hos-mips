#include <proc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <trap.h>
#include <unistd.h>
#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <assert.h>
#include <elf.h>
#include <fs.h>
#include <vfs.h>
#include <sysfile.h>
#include <mbox.h>
#include <mips_io.h>
#include <stdio.h>
#include <mp.h>
#include <resource.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>

//#define DEBUG_PROCESS

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc, 
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:
                                            
  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+ 
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  + 
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep 
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit   
SYS_getpid      : get the process's pid

*/

PLS struct proc_struct *pls_current;
PLS struct proc_struct *pls_idleproc;
struct proc_struct *initproc;

#define current (pls_read(current))
#define idleproc (pls_read(idleproc))

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT          10
#define HASH_LIST_SIZE      (1 << HASH_SHIFT)
#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];
static int nr_process = 0;

static int __do_exit(void);
static int __do_kill(struct proc_struct *proc, int error_code);

// alloc_proc - alloc a proc_struct and init all fields of proc_struct
struct proc_struct *alloc_proc(void)
{
	struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
	if (proc != NULL) {
		proc->state = PROC_UNINIT;
		proc->pid = -1;
		proc->runs = 0;
		proc->kstack = 0;
		proc->need_resched = 0;
		proc->parent = NULL;
		proc->tf = NULL;
		proc->flags = 0;
		proc->need_resched = 0;
		proc->cr3 = boot_cr3;
		memset(&(proc->context), 0, sizeof(struct context));
		memset(proc->name, 0, PROC_NAME_LEN);
		proc->exit_code = 0;
		proc->wait_state = 0;
		list_init(&(proc->run_link));
		list_init(&(proc->list_link));
		proc->time_slice = 0;
		proc->cptr = proc->yptr = proc->optr = NULL;
		event_box_init(&(proc->event_box));
		proc->fs_struct = NULL;
		proc->sem_queue = sem_queue_create();
		proc->pgdir = NULL;
	}
	return proc;
}

// set_proc_name - set the name of proc
char *set_proc_name(struct proc_struct *proc, const char *name)
{
	memset(proc->name, 0, sizeof(proc->name));
	return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *get_proc_name(struct proc_struct *proc)
{
	static char name[PROC_NAME_LEN + 1];
	memset(name, 0, sizeof(name));
	return memcpy(name, proc->name, PROC_NAME_LEN);
}

// set_links - set the relation links of process
static void set_links(struct proc_struct *proc)
{
	list_add(&proc_list, &(proc->list_link));
	proc->yptr = NULL;
	if ((proc->optr = proc->parent->cptr) != NULL) {
		proc->optr->yptr = proc;
	}
	proc->parent->cptr = proc;
	nr_process++;
}

// remove_links - clean the relation links of process
static void remove_links(struct proc_struct *proc)
{
	list_del(&(proc->list_link));
	if (proc->optr != NULL) {
		proc->optr->yptr = proc->yptr;
	}
	if (proc->yptr != NULL) {
		proc->yptr->optr = proc->optr;
	} else {
		proc->parent->cptr = proc->optr;
	}
	nr_process--;
}

// get_pid - alloc a unique pid for process
static int get_pid(void)
{
	static_assert(MAX_PID > MAX_PROCESS);
	struct proc_struct *proc;
	list_entry_t *list = &proc_list, *le;
	static int next_safe = MAX_PID, last_pid = MAX_PID;
	if (++last_pid >= MAX_PID) {
		last_pid = pls_read(lcpu_count);
		goto inside;
	}
	if (last_pid >= next_safe) {
inside:
		next_safe = MAX_PID;
repeat:
		le = list;
		while ((le = list_next(le)) != list) {
			proc = le2proc(le, list_link);
			if (proc->pid == last_pid) {
				if (++last_pid >= next_safe) {
					if (last_pid >= MAX_PID) {
						last_pid = 1;
					}
					next_safe = MAX_PID;
					goto repeat;
				}
			} else if (proc->pid > last_pid
				   && next_safe > proc->pid) {
				next_safe = proc->pid;
			}
		}
	}
	return last_pid;
}

// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load  base addr of "proc"'s new PDT
void proc_run(struct proc_struct *proc)
{
	if (proc != current) {
		bool intr_flag;
		struct proc_struct *prev = current, *next = proc;
		// kprintf("(%d) => %d\n\r", lapic_id, next->pid);
		local_intr_save(intr_flag);
		{
			pls_write(current, proc);
			load_rsp0(next->kstack + KSTACKSIZE);
			set_pagetable(next->pgdir);
			switch_to(&(prev->context), &(next->context));
		}
		local_intr_restore(intr_flag);
	}
}

// hash_proc - add proc into proc hash_list
static void hash_proc(struct proc_struct *proc)
{
	list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
static void unhash_proc(struct proc_struct *proc)
{
	list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *find_proc(int pid)
{
	if (0 < pid && pid < MAX_PID) {
		list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
		while ((le = list_next(le)) != list) {
			struct proc_struct *proc = le2proc(le, hash_link);
			if (proc->pid == pid) {
				return proc;
			}
		}
	}
	return NULL;
}

// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int setup_kstack(struct proc_struct *proc)
{
	struct Page *page = alloc_pages(KSTACKPAGE);
	if (page != NULL) {
		proc->kstack = (uintptr_t) page2kva(page);
		return 0;
	}
	return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void put_kstack(struct proc_struct *proc)
{
	free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - alloc one page as PDT
// XXX, PDT size != PGSIZE!!!
static int setup_pgdir(pgd_t **set_pgdir)
{
	struct Page *page;
	if ((page = alloc_page()) == NULL) {
		return -E_NO_MEM;
	}
	pgd_t *pgdir = page2kva(page);
	memcpy(pgdir, init_pgdir_get(), PGSIZE);
	map_pgdir(pgdir);
	*set_pgdir = pgdir;
	return 0;
}

// put_pgdir - free the memory space of PDT
static void put_pgdir(pgd_t *pgdir)
{
	free_page(kva2page(pgdir));
}

// de_thread - delete this thread "proc" from thread_group list
static void de_thread(struct proc_struct *proc)
{
	if (!list_empty(&(proc->thread_group))) {
		bool intr_flag;
		local_intr_save(intr_flag);
		{
			list_del_init(&(proc->thread_group));
		}
		local_intr_restore(intr_flag);
	}
}

// next_thread - get the next thread "proc" from thread_group list
static struct proc_struct *next_thread(struct proc_struct *proc)
{
	return le2proc(list_next(&(proc->thread_group)), thread_group);
}

static int copy_sem(uint32_t clone_flags, struct proc_struct *proc)
{
	sem_queue_t *sem_queue, *old_sem_queue = current->sem_queue;

	/* current is kernel thread */
	if (old_sem_queue == NULL) {
		return 0;
	}

	if (clone_flags & CLONE_SEM) {
		sem_queue = old_sem_queue;
		goto good_sem_queue;
	}

	int ret = -E_NO_MEM;
	if ((sem_queue = sem_queue_create()) == NULL) {
		goto bad_sem_queue;
	}

	down(&(old_sem_queue->sem));
	ret = dup_sem_queue(sem_queue, old_sem_queue);
	up(&(old_sem_queue->sem));

	if (ret != 0) {
		goto bad_dup_cleanup_sem;
	}

good_sem_queue:
	sem_queue_count_inc(sem_queue);
	proc->sem_queue = sem_queue;
	return 0;

bad_dup_cleanup_sem:
	exit_sem_queue(sem_queue);
	sem_queue_destroy(sem_queue);
bad_sem_queue:
	return ret;
}

static void put_sem_queue(struct proc_struct *proc)
{
	sem_queue_t *sem_queue = proc->sem_queue;
	if (sem_queue != NULL) {
		if (sem_queue_count_dec(sem_queue) == 0) {
			exit_sem_queue(sem_queue);
			sem_queue_destroy(sem_queue);
		}
	}
}

static int copy_signal(uint32_t clone_flags, struct proc_struct *proc)
{
	struct signal_struct *signal, *oldsig = current->signal_info.signal;

	if (oldsig == NULL) {
		return 0;
	}
#if 0
	if (clone_flags & CLONE_THREAD) {
		signal = oldsig;
		goto good_signal;
	}
#endif

	int ret = -E_NO_MEM;
	if ((signal = signal_create()) == NULL) {
		goto bad_signal;
	}

good_signal:
	signal_count_inc(signal);
	proc->signal_info.signal = signal;
	return 0;

bad_signal:
	return ret;
}

// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
static void put_signal(struct proc_struct *proc)
{
	struct signal_struct *sig = proc->signal_info.signal;
	if (sig != NULL) {
		if (signal_count_dec(sig) == 0) {
			signal_destroy(sig);
		}
	}
	proc->signal_info.signal = NULL;
}

static int copy_sighand(uint32_t clone_flags, struct proc_struct *proc)
{
	struct sighand_struct *sighand, *oldsh = current->signal_info.sighand;

	if (oldsh == NULL) {
		return 0;
	}

	if (clone_flags & (CLONE_SIGHAND | CLONE_THREAD)) {
		sighand = oldsh;
		goto good_sighand;
	}

	int ret = -E_NO_MEM;
	if ((sighand = sighand_create()) == NULL) {
		goto bad_sighand;
	}

good_sighand:
	sighand_count_inc(sighand);
	proc->signal_info.sighand = sighand;
	return 0;

bad_sighand:
	return ret;
}

static void put_sighand(struct proc_struct *proc)
{
	struct sighand_struct *sh = proc->signal_info.sighand;
	if (sh != NULL) {
		if (sighand_count_dec(sh) == 0) {
			sighand_destroy(sh);
		}
	}
	proc->signal_info.sighand = NULL;
}

static int copy_fs(uint32_t clone_flags, struct proc_struct *proc)
{
	struct fs_struct *fs_struct, *old_fs_struct = current->fs_struct;
	assert(old_fs_struct != NULL);

	if (clone_flags & CLONE_FS) {
		fs_struct = old_fs_struct;
		goto good_fs_struct;
	}

	int ret = -E_NO_MEM;
	if ((fs_struct = fs_create()) == NULL) {
		goto bad_fs_struct;
	}

	if ((ret = dup_fs(fs_struct, old_fs_struct)) != 0) {
		goto bad_dup_cleanup_fs;
	}

good_fs_struct:
	fs_count_inc(fs_struct);
	proc->fs_struct = fs_struct;
	return 0;

bad_dup_cleanup_fs:
	fs_destroy(fs_struct);
bad_fs_struct:
	return ret;
}

static int copy_pgdir(uint32_t clone_flags, struct proc_struct *proc) {
	pde_t *old_pgdir = current->pgdir;
	pde_t *pgdir;
    /* current is a kernel thread */
	if(current->pgdir == NULL)
		return 0;

    int ret = -E_NO_MEM;
    if (setup_pgdir(&pgdir) != 0)
		return -1;

    proc->pgdir = pgdir;
    proc->cr3 = PADDR(pgdir);
	copy_range(pgdir, old_pgdir, USERBASE, USERTOP, 0);
	return 0;
}

static void put_fs(struct proc_struct *proc)
{
	struct fs_struct *fs_struct = proc->fs_struct;
	if (fs_struct != NULL) {
		if (fs_count_dec(fs_struct) == 0) {
			fs_destroy(fs_struct);
		}
	}
}

// may_killed - check if current thread should be killed, should be called before go back to user space
void may_killed(void)
{
	// killed by other process, already set exit_code and call __do_exit directly
	if (current->flags & PF_EXITING) {
		__do_exit();
	}
}

// do_fork - parent process for a new child process
//    1. call alloc_proc to allocate a proc_struct
//    2. call setup_kstack to allocate a kernel stack for child process
//    3. call copy_mm to dup OR share mm according clone_flag
//    4. call wakup_proc to make the new child process RUNNABLE 
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
	int ret = -E_NO_FREE_PROC;
	struct proc_struct *proc;
	if (nr_process >= MAX_PROCESS) {
		goto fork_out;
	}

	ret = -E_NO_MEM;

	if ((proc = alloc_proc()) == NULL) {
		goto fork_out;
	}

	proc->parent = current;
	list_init(&(proc->thread_group));
	assert(current->wait_state == 0);

	assert(current->time_slice >= 0);
	proc->time_slice = current->time_slice / 2;
	current->time_slice -= proc->time_slice;

	if (setup_kstack(proc) != 0) {
		goto bad_fork_cleanup_proc;
	}
	if (copy_sem(clone_flags, proc) != 0) {
		goto bad_fork_cleanup_kstack;
	}
	if (copy_fs(clone_flags, proc) != 0) {
		goto bad_fork_cleanup_sem;
	}
	if (copy_pgdir(clone_flags, proc) != 0){
		goto bad_fork_cleanup_proc;
	}
	if (copy_signal(clone_flags, proc) != 0) {
		goto bad_fork_cleanup_fs;
	}
	if (copy_sighand(clone_flags, proc) != 0) {
		goto bad_fork_cleanup_signal;
	}
	if (copy_thread(clone_flags, proc, stack, tf) != 0) {
		goto bad_fork_cleanup_sighand;
	}

	proc->tls_pointer = current->tls_pointer;

	bool intr_flag;
	local_intr_save(intr_flag);
	{
		proc->pid = get_pid();
		proc->tid = proc->pid;
		hash_proc(proc);
		set_links(proc);
		if (clone_flags & CLONE_THREAD) {
			list_add_before(&(current->thread_group),
					&(proc->thread_group));
			proc->gid = current->gid;
		} else {
			proc->gid = proc->pid;
		}
	}
	local_intr_restore(intr_flag);

	wakeup_proc(proc);

	ret = proc->pid;
fork_out:
	return ret;
bad_fork_cleanup_sighand:
	put_sighand(proc);
bad_fork_cleanup_signal:
	put_signal(proc);
bad_fork_cleanup_fs:
	put_fs(proc);
bad_fork_cleanup_sem:
	put_sem_queue(proc);
bad_fork_cleanup_kstack:
	put_kstack(proc);
bad_fork_cleanup_proc:
	kfree(proc);
	goto fork_out;
}

// __do_exit - cause a thread exit (use do_exit, do_exit_thread instead)
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
static int __do_exit(void)
{
	if (current == idleproc) {
		panic("idleproc exit.\n\r");
	}
	if (current == initproc) {
		panic("initproc exit.\n\r");
	}

	pde_t *pgdir = current->pgdir;
	if (pgdir != NULL) {
		set_pagetable(NULL);
		put_pgdir(pgdir);
		current->pgdir = NULL;
	}
	put_sighand(current);
	put_signal(current);
	put_fs(current);
	put_sem_queue(current);
	current->state = PROC_ZOMBIE;

	bool intr_flag;
	struct proc_struct *proc, *parent;
	local_intr_save(intr_flag);
	{
		proc = parent = current->parent;
		do {
			if (proc->wait_state == WT_CHILD) {
				wakeup_proc(proc);
			}
			proc = next_thread(proc);
		} while (proc != parent);

		if ((parent = next_thread(current)) == current) {
			parent = initproc;
		}
		de_thread(current);
		while (current->cptr != NULL) {
			proc = current->cptr;
			current->cptr = proc->optr;

			proc->yptr = NULL;
			if ((proc->optr = parent->cptr) != NULL) {
				parent->cptr->yptr = proc;
			}
			proc->parent = parent;
			parent->cptr = proc;
			if (proc->state == PROC_ZOMBIE) {
				if (parent->wait_state == WT_CHILD) {
					wakeup_proc(parent);
				}
			}
		}
	}

	wakeup_queue(&(current->event_box.wait_queue), WT_INTERRUPTED, 1);

	local_intr_restore(intr_flag);

	schedule();
	panic("__do_exit will not return!! %d %d.\n\r", current->pid,
	      current->exit_code);
}

// do_exit - kill a thread group, called by syscall or trap handler
int do_exit(int error_code)
{
	bool intr_flag;
	local_intr_save(intr_flag);
	{
		list_entry_t *list = &(current->thread_group), *le = list;
		while ((le = list_next(le)) != list) {
			struct proc_struct *proc = le2proc(le, thread_group);
			__do_kill(proc, error_code);
		}
	}
	local_intr_restore(intr_flag);
	return do_exit_thread(error_code);
}

// do_exit_thread - kill a single thread
int do_exit_thread(int error_code)
{
	current->exit_code = error_code;
	return __do_exit();
}

static int load_icode_read(int fd, void *buf, size_t len, off_t offset)
{
	int ret;
	if ((ret = sysfile_seek(fd, offset, LSEEK_SET)) != 0) {
		return ret;
	}
	if ((ret = sysfile_read(fd, buf, len)) != len) {
		return (ret < 0) ? ret : -1;
	}
	return 0;
}

static int
map_ph(int fd, struct proghdr *ph,  pde_t *pgdir, uint32_t linker)
{
	int ret = 0;
	struct Page *page;
	pte_perm_t perm = 0;
	ptep_set_u_read(&perm);

	off_t offset = ph->p_offset;
	size_t off, size;
	uintptr_t start = ph->p_va , end, la = ROUNDDOWN(start, PGSIZE);

	end = ph->p_va + ph->p_filesz;
	while (start < end) {
		if ((page = pgdir_alloc_page(pgdir, la, perm)) == NULL) {
			ret = -E_NO_MEM;
			goto bad_cleanup_mmap;
		}
		off = start - la, size = PGSIZE - off, la += PGSIZE;
		if (end < la) {
			size -= la - end;
		}
		if ((ret =
		     load_icode_read(fd, page2kva(page) + off, size,
				     offset)) != 0) {
			goto bad_cleanup_mmap;
		}
		start += size, offset += size;
	}

	end = ph->p_va + ph->p_memsz;

	if (start < la) {
		if (start == end) {
			goto normal_exit;
		}
		off = start + PGSIZE - la, size = PGSIZE - off;
		if (end < la) {
			size -= la - end;
		}
		memset(page2kva(page) + off, 0, size);
		start += size;
		assert((end < la && start == end)
		       || (end >= la && start == la));
	}

	while (start < end) {
		if ((page = pgdir_alloc_page(pgdir, la, perm)) == NULL) {
			ret = -E_NO_MEM;
			goto bad_cleanup_mmap;
		}
		off = start - la, size = PGSIZE - off, la += PGSIZE;
		if (end < la) {
			size -= la - end;
		}
		memset(page2kva(page) + off, 0, size);
		start += size;
	}
normal_exit:
	return 0;
bad_cleanup_mmap:
	return ret;
}

static int load_icode(int fd, int argc, char **kargv, int envc, char **kenvp)
{
	assert(argc >= 0 && argc <= EXEC_MAX_ARG_NUM);
	assert(envc >= 0 && envc <= EXEC_MAX_ENV_NUM);

	int ret = -E_NO_MEM;

	if (setup_pgdir(&current->pgdir) != 0) {
		goto bad_pgdir_cleanup_mm;
	}

	struct Page *page;

	struct elfhdr __elf, *elf = &__elf;
	if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0) {
		goto bad_elf_cleanup_pgdir;
	}

	if (elf->e_magic != ELF_MAGIC) {
		ret = -E_INVAL_ELF;
		goto bad_elf_cleanup_pgdir;
	}

	uint32_t load_address, load_address_flag = 0;

	struct proghdr __ph, *ph = &__ph;
	uint32_t vm_flags, phnum;

	for (phnum = 0; phnum < elf->e_phnum; phnum++) {
		off_t phoff = elf->e_phoff + sizeof(struct proghdr) * phnum;
		if ((ret =
		     load_icode_read(fd, ph, sizeof(struct proghdr),
				     phoff)) != 0) {
			goto bad_cleanup_mmap;
		}

		if (ph->p_type != ELF_PT_LOAD) {
			continue;
		}
		if (ph->p_filesz > ph->p_memsz) {
			ret = -E_INVAL_ELF;
			goto bad_cleanup_mmap;
		}

		if ((ret = map_ph(fd, ph, current->pgdir, 0)) != 0) {
			kprintf("load address: 0x%08x size: %d\n", ph->p_va,
				ph->p_memsz);
			goto bad_cleanup_mmap;
		}
		        off_t offset = ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

        ret = -E_NO_MEM;
		uint32_t perm = PTE_U;
		if (ph->p_flags & ELF_PF_W)
			perm |= PTE_W;
        end = ph->p_va + ph->p_filesz;
        while (start < end) {
            if ((page = pgdir_alloc_page(current->pgdir, la, perm)) == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            if ((ret = load_icode_read(fd, page2kva(page) + off, size, offset)) != 0) {
                goto bad_cleanup_mmap;
            }
            start += size, offset += size;
        }
        end = ph->p_va + ph->p_memsz;

        if (start < la) {
            /* ph->p_memsz == ph->p_filesz */
            if (start == end) {
                continue ;
            }
            off = start + PGSIZE - la, size = PGSIZE - off;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        while (start < end) {
            if ((page = pgdir_alloc_page(current->pgdir, la, perm)) == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
	}
	sysfile_close(fd);

	/* setup user stack */
	uintptr_t i = ROUNDDOWN(USTACKTOP - USTACKSIZE, PGSIZE);
	for(;i < ROUNDUP(USTACKTOP - USTACKSIZE + USTACKSIZE, PGSIZE);i+=PGSIZE){
		assert(pgdir_alloc_page(current->pgdir, i, PTE_USER) != NULL);
	}

	set_pgdir(current, current->pgdir);
	lcr3(PADDR(current->pgdir));

	uintptr_t stacktop = USTACKTOP - argc * PGSIZE;
	char **uargv = (char **)(stacktop - argc * sizeof(char *));
	for (i = 0; i < argc; i++) {
		uargv[i] = strcpy((char *)(stacktop + i * PGSIZE), kargv[i]);
	}
	struct trapframe *tf = current->tf;
	memset(tf, 0, sizeof(struct trapframe));
	tf->tf_epc = elf->e_entry;
	tf->tf_regs.reg_r[MIPS_REG_SP] = USTACKTOP;
	uint32_t status = read_c0_status();
	status &= ~ST0_KSU;
	status |= KSU_USER;
	status |= ST0_EXL;
	tf->tf_status = status;
	tf->tf_regs.reg_r[MIPS_REG_A0] = argc;
	tf->tf_regs.reg_r[MIPS_REG_A1] = (uint32_t) uargv;

	ret = 0;
out:
	return ret;
bad_cleanup_mmap:
bad_elf_cleanup_pgdir:
	put_pgdir(current->pgdir);
bad_pgdir_cleanup_mm:
bad_mm:
	goto out;
}

static void put_kargv(int argc, char **kargv)
{
	while (argc > 0) {
		kfree(kargv[--argc]);
	}
}

static int
copy_kargv(char **kargv, const char **argv, int max_argc,
	   int *argc_store)
{
	int i, ret = -E_INVAL;
	if (!argv) {
		*argc_store = 0;
		return 0;
	}
	char *argv_k;
	for (i = 0; i < max_argc; i++) {
		if (!memcpy(&argv_k, argv + i, sizeof(char *)))
			goto failed_cleanup;
		if (!argv_k)
			break;
		char *buffer;
		if ((buffer = kmalloc(EXEC_MAX_ARG_LEN + 1)) == NULL) {
			goto failed_nomem;
		}
		if (!copy_string(buffer, argv_k, EXEC_MAX_ARG_LEN + 1)) {
			kfree(buffer);
			goto failed_cleanup;
		}
		kargv[i] = buffer;
	}
	*argc_store = i;
	return 0;

failed_nomem:
	ret = -E_NO_MEM;
failed_cleanup:
	put_kargv(i, kargv);
	return ret;
}

int do_execve(const char *filename, const char **argv, const char **envp)
{
	static_assert(EXEC_MAX_ARG_LEN >= FS_MAX_FPATH_LEN);

	char local_name[PROC_NAME_LEN + 1];
	memset(local_name, 0, sizeof(local_name));

	char *kargv[EXEC_MAX_ARG_NUM], *kenvp[EXEC_MAX_ENV_NUM];
	const char *path;

	int ret = -E_INVAL;
	snprintf(local_name, sizeof(local_name), "<null> %d", current->pid);

	int argc = 0, envc = 0;
	if ((ret = copy_kargv(kargv, argv, EXEC_MAX_ARG_NUM, &argc)) != 0) {
		return ret;
	}
	if ((ret = copy_kargv(kenvp, envp, EXEC_MAX_ENV_NUM, &envc)) != 0) {
		put_kargv(argc, kargv);
		return ret;
	}
	path = filename;

	int fd;
	if ((ret = fd = sysfile_open(path, O_RDONLY)) < 0) {
		goto execve_exit;
	}

	put_sem_queue(current);

	ret = -E_NO_MEM;
	/* init signal */
	put_sighand(current);
	if ((current->signal_info.sighand = sighand_create()) == NULL) {
		goto execve_exit;
	}
	sighand_count_inc(current->signal_info.sighand);

	put_signal(current);
	if ((current->signal_info.signal = signal_create()) == NULL) {
		goto execve_exit;
	}
	signal_count_inc(current->signal_info.signal);

	if ((current->sem_queue = sem_queue_create()) == NULL) {
		goto execve_exit;
	}
	sem_queue_count_inc(current->sem_queue);

	if ((ret = load_icode(fd, argc, kargv, envc, kenvp)) != 0) {
		goto execve_exit;
	}

	set_proc_name(current, local_name);

	put_kargv(argc, kargv);
	put_kargv(envc, kenvp);
	return 0;

execve_exit:
	put_kargv(argc, kargv);
	put_kargv(envc, kenvp);
/* exec should return -1 if failed */
	//return ret;
	do_exit(ret);
	panic("already exit: %e.\n\r", ret);
}

// do_yield - ask the scheduler to reschedule
int do_yield(void)
{
	current->need_resched = 1;
	return 0;
}

// do_wait - wait one OR any children with PROC_ZOMBIE state, and free memory space of kernel stack
//         - proc struct of this child.
// NOTE: only after do_wait function, all resources of the child proces are free.
int do_wait(int pid, int *code_store)
{
	struct proc_struct *proc;//, *cproc;
	bool intr_flag, haskid;
repeat:
	//cproc = current;
	haskid = 0;
	if (pid != 0) {
    	proc = find_proc(pid);
            if (proc != NULL && proc->parent == current) {
         	haskid = 1;
                if (proc->state == PROC_ZOMBIE) {
                   goto found;
                }
            }
        }
        else {
            proc = current->cptr;
            for (; proc != NULL; proc = proc->optr) {
                haskid = 1;
                if (proc->state == PROC_ZOMBIE) {
                   goto found;
                }
            }
        }
	if (haskid) {
		current->state = PROC_SLEEPING;
		current->wait_state = WT_CHILD;
		schedule();
		may_killed();
		goto repeat;
	}
	return -E_BAD_PROC;

found:
	if (proc == idleproc || proc == initproc) {
		panic("wait idleproc or initproc.\n\r");
	}
	int exit_code = proc->exit_code;
	local_intr_save(intr_flag);
	{
		unhash_proc(proc);
		remove_links(proc);
	}
	local_intr_restore(intr_flag);
	put_kstack(proc);
	kfree(proc);

	int ret = 0;
	if (code_store != NULL) {
		{
			if (!memcpy(code_store, &exit_code, sizeof(int))) {
				ret = -E_INVAL;
			}
		}
	}
	return ret;
}

int do_linux_waitpid(int pid, int *code_store)
{
	struct proc_struct *proc, *cproc;
	bool intr_flag, haskid;
repeat:
	cproc = current;
	haskid = 0;
	if (pid > 0) {
		proc = find_proc(pid);
		if (proc != NULL) {
			do {
				if (proc->parent == cproc) {
					haskid = 1;
					if (proc->state == PROC_ZOMBIE) {
						goto found;
					}
					break;
				}
				cproc = next_thread(cproc);
			} while (cproc != current);
		}
	}
	/* we do NOT have group id, so.. */
	else if (pid == 0 || pid == -1) {	/* pid == 0 */
		do {
			proc = cproc->cptr;
			for (; proc != NULL; proc = proc->optr) {
				haskid = 1;
				if (proc->state == PROC_ZOMBIE) {
					goto found;
				}
			}
			cproc = next_thread(cproc);
		} while (cproc != current);
	} else {		//pid<-1
		//TODO
		return -E_INVAL;
	}
	if (haskid) {
		current->state = PROC_SLEEPING;
		current->wait_state = WT_CHILD;
		schedule();
		may_killed();
		goto repeat;
	}
	return -E_BAD_PROC;

found:
	if (proc == idleproc || proc == initproc) {
		panic("wait idleproc or initproc.\n\r");
	}
	int exit_code = proc->exit_code;
	int return_pid = proc->pid;
	local_intr_save(intr_flag);
	{
		unhash_proc(proc);
		remove_links(proc);
	}
	local_intr_restore(intr_flag);
	put_kstack(proc);
	kfree(proc);

	int ret = 0;
	if (code_store != NULL) {
		{
			int status = exit_code << 8;
			if (!memcpy(code_store, &status, sizeof(int))) {
				ret = -E_INVAL;
			}
		}
	}
	return (ret == 0) ? return_pid : ret;
}

// __do_kill - kill a process with PCB by set this process's flags with PF_EXITING
static int __do_kill(struct proc_struct *proc, int error_code)
{
	if (!(proc->flags & PF_EXITING)) {
		proc->flags |= PF_EXITING;
		proc->exit_code = error_code;
		if (proc->wait_state & WT_INTERRUPTED) {
			wakeup_proc(proc);
		}
		return 0;
	}
	return -E_KILLED;
}

// do_kill - kill process with pid
int do_kill(int pid, int error_code)
{
	struct proc_struct *proc;
	if ((proc = find_proc(pid)) != NULL) {
		return __do_kill(proc, error_code);
	}
	return -E_INVAL;
}

// do_sleep - set current process state to sleep and add timer with "time"
//          - then call scheduler. if process run again, delete timer first.
//      time is jiffies
int do_sleep(unsigned int time)
{
	assert(!ucore_in_interrupt());
	if (time == 0) {
		return 0;
	}
	bool intr_flag;
	local_intr_save(intr_flag);
	timer_t __timer, *timer = timer_init(&__timer, current, time);
	current->state = PROC_SLEEPING;
	current->wait_state = WT_TIMER;
	add_timer(timer);
	local_intr_restore(intr_flag);

	schedule();

	del_timer(timer);
	return 0;
}

int do_linux_sleep(const struct linux_timespec __user * req,
		   struct linux_timespec __user * rem)
{
	struct linux_timespec kts;
	if (!memcpy(&kts, req, sizeof(struct linux_timespec))) {
		return -E_INVAL;
	}
	long msec = kts.tv_sec * 1000 + kts.tv_nsec / 1000000;
	if (msec < 0)
		return -E_INVAL;
#ifdef UCONFIG_HAVE_LINUX_DDE_BASE
	unsigned long j = msecs_to_jiffies(msec);
#else
	unsigned long j = msec / 10;
#endif
	//kprintf("do_linux_sleep: sleep %d msec, %d jiffies\n\r", msec, j);
	int ret = do_sleep(j);
	if (rem) {
		memset(&kts, 0, sizeof(struct linux_timespec));
		if (!memcpy(rem, &kts, sizeof(struct linux_timespec))) {
			return -E_INVAL;
		}
	}
	return ret;
}

// kernel_execve - do SYS_exec syscall to exec a user program scalled by user_main kernel_thread
int kernel_execve(const char *name, const char **argv, const char **kenvp)
{
	int argc = 0, ret;

	while (argv[argc] != NULL) {
		argc++;
	}
	//panic("unimpl");
	asm volatile ("la $v0, %1;\n"	/* syscall no. */
		      "move $a0, %2;\n"
		      "move $a1, %3;\n"
		      "move $a2, %4;\n"
		      "move $a3, %5;\n"
		      "syscall;\n" "nop;\n" "move %0, $v0;\n":"=r" (ret)
		      :"i"( /*T_SYSCALL+ */ SYS_exec), "r"(name), "r"(argv),
		      "r"(kenvp), "r"(argc)
		      :"a0", "a1", "a2", "a3", "v0");
	return ret;
}
#define __KERNEL_EXECVE(name, path, ...) ({                         \
            const char *argv[] = {path, ##__VA_ARGS__, NULL};       \
            const char *envp[] = {"PATH=/bin/", NULL};              \
            kprintf("kernel_execve: pid = %d, name = \"%s\".\n\r",    \
                    current->pid, name);                            \
            kernel_execve(path, argv, envp);                              \
        })

#define KERNEL_EXECVE(x, ...)                   __KERNEL_EXECVE(#x, #x, ##__VA_ARGS__)

#define KERNEL_EXECVE2(x, ...)                  KERNEL_EXECVE(x, ##__VA_ARGS__)

#define __KERNEL_EXECVE3(x, s, ...)             KERNEL_EXECVE(x, #s, ##__VA_ARGS__)

#define KERNEL_EXECVE3(x, s, ...)               __KERNEL_EXECVE3(x, s, ##__VA_ARGS__)

// user_main - kernel thread used to exec a user program
static int user_main(void *arg)
{
	sysfile_open("stdin:", O_RDONLY);
	sysfile_open("stdout:", O_WRONLY);
	sysfile_open("stdout:", O_WRONLY);
#ifdef UNITTEST
#ifdef TESTSCRIPT
	KERNEL_EXECVE3(UNITTEST, TESTSCRIPT);
#else
	KERNEL_EXECVE2(UNITTEST);
#endif
#else
	__KERNEL_EXECVE("/bin/sh", "/bin/sh");
#endif
	kprintf("user_main execve failed, no /bin/sh?.\n\r");
}

// init_main - the second kernel thread used to create kswapd_main & user_main kernel threads
static int init_main(void *arg)
{
	int ret;
#ifdef DEBUG_PROCESS
	kprintf("enter vfs_set_bootfs\n\r");
#endif
	if ((ret = vfs_set_bootfs("disk0:")) != 0) {
		panic("set boot fs failed: %e.\n\r", ret);
	}
#ifdef DEBUG_PROCESS
	kprintf("exit vfs_set_bootfs\n\r");
#endif
	size_t nr_used_pages_store = nr_used_pages();

	unsigned int nr_process_store = nr_process;

#ifdef DEBUG_PROCESS
	kprintf("create thread\n\r");
#endif
	int pid = ucore_kernel_thread(user_main, NULL, 0);
	if (pid <= 0) {
		panic("create user_main failed.\n\r");
	}
#ifdef DEBUG_PROCESS
	kprintf("sche thread\n\r");
#endif
	while (do_wait(0, NULL) == 0) {
		if (nr_process_store == nr_process) {
			break;
		}
		schedule();
	}

	mbox_cleanup();
	fs_cleanup();

	kprintf("all user-mode processes have quit.\n\r");
	assert(nr_process == 1 + pls_read(lcpu_count));
	assert(nr_used_pages_store == nr_used_pages());
	kprintf("init check memory pass.\n\r");
	return 0;
}

// proc_init - set up the first kernel thread idleproc "idle" by itself and 
//           - create the second kernel thread init_main
void proc_init(void)
{
	int i;
	int lcpu_idx = pls_read(lcpu_idx);
	int lapic_id = pls_read(lapic_id);
	int lcpu_count = pls_read(lcpu_count);

	list_init(&proc_list);
	for (i = 0; i < HASH_LIST_SIZE; i++) {
		list_init(hash_list + i);
	}

	pls_write(idleproc, alloc_proc());
	if (idleproc == NULL) {
		panic("cannot alloc idleproc.\n\r");
	}

	idleproc->pid = lcpu_idx;
	idleproc->state = PROC_RUNNABLE;
	// XXX
	// idleproc->kstack = (uintptr_t)bootstack;
	idleproc->need_resched = 1;
	idleproc->tf = NULL;
	if ((idleproc->fs_struct = fs_create()) == NULL) {
		panic("create fs_struct (idleproc) failed.\n\r");
	}
	fs_count_inc(idleproc->fs_struct);

	char namebuf[32];
	snprintf(namebuf, 32, "idle/%d", lapic_id);

	set_proc_name(idleproc, namebuf);
	nr_process++;

	pls_write(current, idleproc);

	int pid = ucore_kernel_thread(init_main, NULL, 0);
	if (pid <= 0) {
		panic("create init_main failed.\n\r");
	}

	initproc = find_proc(pid);
	set_proc_name(initproc, "kinit");

	assert(idleproc != NULL && idleproc->pid == lcpu_idx);
	assert(initproc != NULL && initproc->pid == lcpu_count);
}

void proc_init_ap(void)
{
	int lcpu_idx = pls_read(lcpu_idx);
	int lapic_id = pls_read(lapic_id);

	pls_write(idleproc, alloc_proc());
	if (idleproc == NULL) {
		panic("cannot alloc idleproc.\n\r");
	}

	idleproc->pid = lcpu_idx;
	idleproc->state = PROC_RUNNABLE;
	// XXX
	// idleproc->kstack = (uintptr_t)bootstack;
	idleproc->need_resched = 1;
	idleproc->tf = NULL;
	if ((idleproc->fs_struct = fs_create()) == NULL) {
		panic("create fs_struct (idleproc) failed.\n\r");
	}
	fs_count_inc(idleproc->fs_struct);

	char namebuf[32];
	snprintf(namebuf, 32, "idle/%d", lapic_id);

	set_proc_name(idleproc, namebuf);
	nr_process++;

	pls_write(current, idleproc);

	assert(idleproc != NULL && idleproc->pid == lcpu_idx);
}

int do_linux_ugetrlimit(int res, struct linux_rlimit *__user __limit)
{
	struct linux_rlimit limit;
	switch (res) {
	case RLIMIT_STACK:
		limit.rlim_cur = USTACKSIZE;
		limit.rlim_max = USTACKSIZE;
		break;
	default:
		return -E_INVAL;
	}
	int ret = 0;
	if (!memcpy(__limit, &limit, sizeof(struct linux_rlimit))) {
		ret = -E_INVAL;
	}
	return ret;
}
// cpu_idle - at the end of kern_init, the first kernel thread idleproc will do below works
void cpu_idle(void)
{
	while (1) {
		if (current->need_resched) {
			schedule();
		}
	}
}

// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void forkret(void)
{				
	forkrets(current->tf);
}

// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
int
copy_thread(uint32_t clone_flags, struct proc_struct *proc, uintptr_t esp,
	    struct trapframe *tf)
{
	proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
	*(proc->tf) = *tf;
	proc->tf->tf_regs.reg_r[MIPS_REG_V0] = 0;
	if (esp == 0)		//a kernel thread
		esp = (uintptr_t) proc->tf - 32;
	proc->tf->tf_regs.reg_r[MIPS_REG_SP] = esp;
	proc->context.sf_ra = (uintptr_t) forkret;
	proc->context.sf_sp = (uintptr_t) (proc->tf) - 32;
	return 0;
}

int ucore_kernel_thread(int (*fn) (void *), void *arg, uint32_t clone_flags)
{
	return kernel_thread(fn, arg, clone_flags);
}

// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to 
//       proc->tf in do_fork-->copy_thread function
int kernel_thread(int (*fn) (void *), void *arg, uint32_t clone_flags)
{
	struct trapframe tf;
	memset(&tf, 0, sizeof(struct trapframe));
	tf.tf_regs.reg_r[MIPS_REG_A0] = (uint32_t) arg;
	tf.tf_regs.reg_r[MIPS_REG_A1] = (uint32_t) fn;
	tf.tf_regs.reg_r[MIPS_REG_V0] = 0;
	//TODO
	tf.tf_status = read_c0_status();
	tf.tf_status &= ~ST0_KSU;
	tf.tf_status |= ST0_IE;
	tf.tf_status |= ST0_EXL;
	tf.tf_regs.reg_r[MIPS_REG_GP] = __read_reg($28);
	tf.tf_epc = (uint32_t) kernel_thread_entry;
	return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

/* $a0 = arg, $a1 = func
 * see proc.c:kernel_thread
 */
void kernel_thread_entry(void)
{
	__asm__ __volatile__("addiu $sp, $sp, -16\n");
	__asm__ __volatile__("jal $a1\n");
	__asm__ __volatile__("nop\n");
	__asm__ __volatile__("move $a0, $v0\n");
	__asm__ __volatile__("la  $t0, do_exit\n");
	__asm__ __volatile__("jal $t0\n");
	__asm__ __volatile__("nop\n");
}
