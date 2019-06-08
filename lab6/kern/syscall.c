/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, s, len, PTE_U | PTE_P);
	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.
	// LAB 4: Your code here.
	struct Env *e;
	int r = env_alloc(&e, curenv->env_id);
	if (r)
		return r;
	e->env_status = ENV_NOT_RUNNABLE;
	e->env_tf = curenv->env_tf;
	e->brk = curenv->brk;
	e->env_tf.tf_regs.reg_eax = 0;
	
	return e->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
	struct Env *e;
	int r = envid2env(envid, &e, 1);
	if (r < 0) 
	return r;

	if (status == ENV_RUNNABLE || status == ENV_NOT_RUNNABLE)
		e->env_status = status;
	else
		return -E_INVAL;

	return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3), interrupts enabled, and IOPL of 0.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	struct Env *e;
	int r;
	if ((r = envid2env(envid, &e, 1)) < 0) return r;

	user_mem_check(e, tf, sizeof(struct Trapframe), PTE_W);
	tf->tf_cs = GD_UT | 3;
	tf->tf_eflags |= FL_IF;
	tf->tf_eflags &= (0xFFFFFFFF & FL_IOPL_0);
	e->env_tf = *tf;

	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env *e;
	int r = envid2env(envid, &e, 1);
	if (r < 0) return r;

	e->env_pgfault_upcall = func;
	return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!
	// LAB 4: Your code here.
	int r = 0;
	struct Env *e;
	r = envid2env(envid, &e, 1);
	if (r < 0) return r;  // -E_BAD_ENV: environment doesn't exists

	if ((uintptr_t)va >= UTOP) return -E_INVAL;
	if ((uintptr_t)va != ROUNDDOWN((uintptr_t)va, PGSIZE)) return -E_INVAL;  // not page-aligned

	if (!(perm & (PTE_U | PTE_P))) return -E_INVAL;   // PTE_U | PTE_P not set
	if (perm & ~PTE_SYSCALL) return -E_INVAL;         // perm is inappropriate

	struct PageInfo *p = page_alloc(ALLOC_ZERO);
	if (!p) return -E_NO_MEM;

	r = page_insert(e->env_pgdir, p, va, perm);
	if (r < 0) {
		page_free(p);
		return r;
	}

	return 0;
}

static int
sys_exec(void *binary, char **argv)
{
	// TODO: syscall exec

	return env_exec(binary, argv);
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.
	// LAB 4: Your code here.
	int r = 0;
	struct Env *src_env, *dst_env;
	r = envid2env(srcenvid, &src_env, 1);
	if (r < 0) return r;
	r = envid2env(dstenvid, &dst_env, 1);
	if (r < 0) return r;

	if (!(perm & (PTE_U | PTE_P))) return -E_INVAL;   // PTE_U | PTE_P not set
	if (perm & ~PTE_SYSCALL) return -E_INVAL;    // inapproriate permission

	if ((uintptr_t)srcva >= UTOP) return -E_INVAL;          // srcva in kernel space
	if ((uintptr_t)srcva != ROUNDDOWN((uintptr_t)srcva, PGSIZE)) return -E_INVAL;  // srcva not page-aligned
	if ((uintptr_t)dstva >= UTOP) return -E_INVAL;          // dstva in kernel space
	if ((uintptr_t)dstva != ROUNDDOWN((uintptr_t)dstva, PGSIZE)) return -E_INVAL;  // dstva not page aligned

	pte_t *srcpte, *dstpte;
	struct PageInfo *pp = page_lookup(src_env->env_pgdir, srcva, &srcpte);
	if (!pp) return -E_INVAL;                    // srcva is not mapped in srcenvid's address space
	if ((*srcpte & PTE_W) == 0 && (perm & PTE_W) != 0)       // read-only page
		return -E_INVAL;

	r = page_insert(dst_env->env_pgdir, pp, dstva, perm);
	if (r < 0) return r;

	return 0;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	struct Env *e;
	int r = 0;
	r = envid2env(envid, &e, 1);
	if (r < 0) return r;

	if ((uintptr_t)va >= UTOP) return -E_INVAL;
	if (va != ROUNDDOWN(va, PGSIZE)) return -E_INVAL;

	page_remove(e->env_pgdir, va);
	return 0;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	int r;

	struct Env *remoteEnv;
	r = envid2env(envid, &remoteEnv, 0);
	if (r < 0) return r;
	if (!remoteEnv->env_ipc_recving)  // remote env is not block in recving
		return -E_IPC_NOT_RECV;

	if (srcva < (void *)UTOP) {
		
		if ((srcva != ROUNDDOWN(srcva, PGSIZE)) ||            // page-aligned ?
				((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P)) ||  // PTE_P and PTE_U set ?
				(perm & ~PTE_SYSCALL))                            // approriate perm ?
			return -E_INVAL;

		// src is not mapped in caller's address space
		pte_t *entry;
		struct PageInfo *p = page_lookup(curenv->env_pgdir, srcva, &entry);
		if (!p ||     																				// srcva not mapped
				((perm & PTE_W) && !(*entry & PTE_W)))            // read-only
			return -E_INVAL;

		if (remoteEnv->env_ipc_dstva < (void *)UTOP) {
			r = page_insert(remoteEnv->env_pgdir, p, remoteEnv->env_ipc_dstva, perm);
			if (r < 0) return r;
		}
	}

	remoteEnv->env_ipc_recving = false;
	remoteEnv->env_ipc_from = curenv->env_id;
	remoteEnv->env_ipc_value = value;
	remoteEnv->env_ipc_perm = perm;

	remoteEnv->env_tf.tf_regs.reg_eax = 0;
	remoteEnv->env_status = ENV_RUNNABLE;

	return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	if ((uintptr_t)dstva < UTOP && dstva != ROUNDDOWN(dstva, PGSIZE))
		return -E_INVAL;

	curenv->env_ipc_recving = true;
	curenv->env_ipc_dstva = dstva;

	curenv->env_status = ENV_NOT_RUNNABLE;

	sched_yield();
	return 0;
}

static int
sys_map_kernel_page(void* kpage, void* va)
{
    int r;
    struct PageInfo* p = pa2page(PADDR(kpage));
    if (p == NULL)
        return E_INVAL;
    r = page_insert(curenv->env_pgdir, p, va, PTE_U | PTE_W);
    return r;
}

static int
sys_sbrk(uint32_t inc)
{
    // LAB3: your code here.

	int r;
	// the page brk resides is allocated already, 
	// no need to alloc again!
	uintptr_t vfloor = ROUNDUP(curenv->brk, PGSIZE);
	uintptr_t vceil = ROUNDUP(curenv->brk + inc, PGSIZE);
	for (uintptr_t i = vfloor; i != vceil; i += PGSIZE) {
		struct PageInfo *p = page_alloc(!ALLOC_ZERO);
		if (!p)
			panic("region alloc: allocation failed for %08ld\n", (uintptr_t)i);
		
		r = page_insert(curenv->env_pgdir, p, (void *)i, PTE_U | PTE_W);
		if (r != 0)
			panic("region alloc: page insert failed, %e", r);
		
	}

	curenv->brk += inc;
	return curenv->brk;
}

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	return time_msec();
}

int
sys_net_send(const void *buf, uint32_t len)
{
	// LAB 6: Your code here.
	// Check the user permission to [buf, buf + len]
	// Call e1000_tx to send the packet
	// Hint: e1000_tx only accept kernel virtual address
	int r= user_mem_check(curenv, buf, len, PTE_U | PTE_P);
	if (r < 0) return r;

	// cprintf("\033[32m[INFO]\033[0m sys_net_send(buf = %08p, len = %u)\n", buf, len);

	r = e1000_tx(buf, len);

	return r;
}

int
sys_net_recv(void *buf, uint32_t len)
{
	// LAB 6: Your code here.
	// Check the user permission to [buf, buf + len]
	// Call e1000_rx to fill the buffer
	// Hint: e1000_rx only accept kernel virtual address
	int r = 0;
	// cprintf("\033[34m[INFO]\033[0m sys_net_recv(buf = %08p, len = %u)\n", buf, len);
	if ((r = user_mem_check(curenv, buf, len, PTE_U | PTE_P | PTE_W)) < 0) return r;
	r = e1000_rx(buf, len);
	
	return r;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	switch (syscallno) {
	case SYS_cputs:
		sys_cputs((const char *)a1, (size_t)a2);
		return 0;
	case SYS_cgetc:
		return sys_cgetc();
	case SYS_getenvid:
		return sys_getenvid();
	case SYS_env_destroy:
		return sys_env_destroy((envid_t) a1);
	case SYS_map_kernel_page:
		return sys_map_kernel_page((void *)a1, (void *)a2);
	case SYS_sbrk:
		return sys_sbrk(a1);
	case SYS_yield:
		sys_yield();
		return 0;
	case SYS_exofork:
		return sys_exofork();
	case SYS_env_set_status:
		return sys_env_set_status((envid_t)a1, (int)a2);
	case SYS_page_alloc:
		return sys_page_alloc((envid_t)a1, (void *)a2, (int)a3);
	case SYS_page_map:
		return sys_page_map((envid_t)a1, (void *)a2, (envid_t)a3, (void *)a4, (int)a5);
	case SYS_page_unmap:
		return sys_page_unmap((envid_t)a1, (void *)a2);
	case SYS_env_set_pgfault_upcall:
		return sys_env_set_pgfault_upcall((envid_t)a1, (void *)a2);
	case SYS_ipc_try_send:
		return sys_ipc_try_send((envid_t)a1, (uint32_t)a2, (void *)a3, (unsigned int)a4);
	case SYS_ipc_recv:
		return sys_ipc_recv((void *)a1);
	case SYS_env_set_trapframe:
		return sys_env_set_trapframe((envid_t)a1, (struct Trapframe *)a2);
	case SYS_exec: 
		return sys_exec((void *)a1, (char **)a2);
	case SYS_time_msec:
		return sys_time_msec();
	case SYS_net_send:
		return sys_net_send((const void *)a1, (uint32_t)(a2));
	case SYS_net_recv:
		return sys_net_recv((void *)a1, (uint32_t)a2);
	case NSYSCALLS:
	default:
		return -E_INVAL;
	}
}
