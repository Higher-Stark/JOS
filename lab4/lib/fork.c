// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	extern volatile pte_t uvpt[];
	pte_t entry = uvpt[PGNUM(addr)];
	if ((err & FEC_WR) == 0)
		panic("pgfault: fault access is expect to be write");
	// if ((entry & PTE_P) == 0)
	// 	panic("pgfault: page doesn't exists");
	if ((entry & PTE_COW) == 0)
		panic("pgfault: COW flag not found, fault va %08p", addr);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	r = sys_page_alloc(0, PFTEMP, PTE_U | PTE_W);
	if (r < 0) 
	panic("pgfault: sys_page_alloc error, %e", r);

	uint8_t *pgaddr = ROUNDDOWN(addr, PGSIZE);
	
	memmove(PFTEMP, pgaddr, PGSIZE);

	r = sys_page_map(0, PFTEMP, 0, pgaddr, PTE_U | PTE_W);
	if (r < 0) 
	panic("pgfault: sys_page_map PFTEMP to pgaddr error, %e", r);

	r = sys_page_unmap(0, PFTEMP);
	if (r < 0) 
	panic("pgfault: sys_page_unmap PFTEMP error, %e", r);

	// LAB 4: Your code here.

	// panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	extern volatile pte_t uvpt[];
	uint8_t *addr;
	addr = (uint8_t *)(pn * PGSIZE);
	// cprintf("duppage begin: %08x\n", (uintptr_t)addr);

	const int perm = PTE_U | PTE_P;

	if (uvpt[pn] & (PTE_W | PTE_COW)) {
		r = sys_page_map(0, addr, envid, addr, PTE_COW | perm);
		if (r < 0) return r;
		r = sys_page_map(0, addr, 0, addr, PTE_COW | perm);
		if (r < 0) return r;
	}
	else {
		r = sys_page_map(0, addr, envid, addr, perm);
		if (r < 0) return r;
	}
	// panic("duppage not implemented");
	// cprintf("duppage end\n");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int r;
	// set parent env page fault handler
	set_pgfault_handler(pgfault);

	envid_t childid = sys_exofork();
	if (childid < 0)
		panic("fork: %e", childid);
	if (childid == 0) {
		// I am a child
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// We are the parent
	// copy address space
	extern volatile pde_t uvpd[];
	extern volatile pte_t uvpt[];

	for (int i = PDX(UTEXT); i <= PDX(USTACKTOP); ++i) {
		
		if (uvpd[i] & PTE_P) {

			for (int j = 0; j != NPTENTRIES; j++) {

				const unsigned int uxpn = PGNUM(UXSTACKTOP - PGSIZE);
				unsigned int pgnum = PGNUM(PGADDR(i, j, 0));

				if ((uvpt[pgnum] & (PTE_P | PTE_U)) == (PTE_U | PTE_P) && pgnum != uxpn)
					if ((r = duppage(childid, pgnum)) < 0)
						panic("fork: duppage error, %e", r);

			}

		}

	}

	// set page fault handler for child env
	extern void _pgfault_upcall(void);

	r = sys_page_alloc(childid, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W);
	if (r < 0)
		panic("fork: allocate user exception stack for child env error, %e", r);

	r = sys_env_set_pgfault_upcall(childid, _pgfault_upcall);
	if (r < 0)
		panic("fork: sys_env_set_pgfault_upcall error, %e", r);

	// set child env RUNNABLE
	if ((r = sys_env_set_status(childid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return childid;

	// panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
