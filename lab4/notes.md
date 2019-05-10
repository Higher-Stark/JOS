# Lab 4 notes

## Brief introduction

This lab makes JOS capable of running multiple CPUs and multitasking, though only SMP(symmetric multiprocessing) model supported.  

## Part A

### Goals

* Multiprocessor support
    We only support SMP (symmetric multiprocessing) model, a multiprocessor in which all CPUs have equivalent access to the system resources.
* Round-Robin scheduling
    At this part, environment switches only when the active environment relinquishes the CPU voluntarily.
* Basic environment management system calls
    Allow user to create additional environments

### Exercise 1

As CPU accesses its LAPIC by MMIO, and MMIO base address in virtual memory space is the same for all the CPU, we just map [pa, pa + size] to [MMIOBASE, MMIOBASE + size] in kernel page directory. The space should be writable by kernel but not user, and it shouldn't be cached but just writes through.

### Exercise 2

`boot_aps()` loads entry code into physical memory starting at MPENTRY_PADDR, then starts other CPUs to execute the entry code one by one. 
The entry code is quite similiar to `boot/boot.S`, mainly initializatin. 
When all CPUs has been started, the BSP returns from `boot_aps()`.

As entry code is loaded into MPENTRY_PAADR, when initializing pages, the page at MPENTRY_PADDR should be marked not free.

### Question 1

As code in `kern/mpentry.S` is loaded to virtual address at `MPENTRY_PADDR`, and linker makes no promise that the code is linked to `MPENTRY_PADDR`, the code must calculate the absolute addresses of its symbols. 
If no `MPBOOTPHYS`, the absolute addresses of symbols is likely to point to somewhere else. APs can't be initialized properly then and the BSP will be blocked.

Entry of code in `boot/boot.S` is assigned to `0x7c00`, as `boot/Makefrag` tells linkder. So `boot/boot.S` has no need for calcalations like `MPBOOTPHYS(s)`.

### Exercise 3

`percpu_kstacks` prepared `NCPU * KSTKSIZE` bytes space for `NCPU` CPU's kernel stacks. 
As `memlayout.h` shows, kernel stack for CPU i is located at `[KSTACKTOP - i * (KSTKSIZE + KSTKGAP) - KSTKSIZE, KSTACKTOP - i *(KSTKSIZE + KSTKGAP)]`. And `KSTKGAP` bytes below CPU i's kernel stack is not backed by physical memory thus fault when the kernel stack overflows, making it the kernel stack guard.

So we just need to map `[KSTACKTOP - i * (KSTKSIZE + KSTKGAP) - KSTKSIZE, KSTACKTOP - i *(KSTKSIZE + KSTKGAP)]` to the physical address of `percpu_kstacks[i]`.

### Exercise 4

As each CPU needs a TSS, we just make each CPU's TSS pointed to a unique TSS. And the global `ts` is deprecated.

__But you may find it not working, that's to say, finding other CPUs won't boot and BSP reboots. If you are confident about the correctness of the implementation, go check if you enable the PSE bit, which is a challenge in the former lab. Disable the PSE bit and just use 4K mapping, APs should boot normally. Why? Good question, I will try to find it out later.__

Why enable PSE failed to boot APs and cause rebooting of BSP? My assumption is when APs entered protected mode and turn on paging, the address is expected to be interpreted by two-level page table. But the kernel space is mapped with large page, the MMU can't retrieve the expected data from memory and raise fault. Probably the failure is reported to BSP and causes the BSP to reboot.

### Exercise 5

```C
lock_kernel();

// Starting non-boot CPUs
boot_aps();
```

Because all processors share one kernel, no APs should take control of the kernel before BSP yields control on kernel. 

For processor state is only changed by kernel, if a processor is halted, it is in ring 0. To start up, grap the kernel lock first.
But if trapped from user level, the processor must grab the lock first, then handles the trap.

In `env_run()`, the processor is about to leave the kernel level to user level. 
The operations to change `curenv`, `cr3` must be in kernel modes, as `env_pop_tf()` is a non-return function, we release the kernel lock before the call.

### Question 2

The big kernel lock prevents from two processors to execute kernel code, but some operations can't be stopped with the lock, for example, trap. When trap occurs, the hardware automaticly pushes some registers onto the kernel stack, whichever processor holds the lock. If only one kernel stack, the origin trap frame will be overwritten.

### Exercise 6

Round Robin scheduling treats each environment equally. For now, the scheduling only happens when an environment voluntarily gives up the processor or an environment completes.

We just record the last running environment in `curenv`, and choose an environment in the back or start from the head of the `envs`.

### Question 3

For `e` is in kernel space, and every environment has the same memory mapping to kernel space. The switch of addressing doesn't affect memory to kernel space text nor data.

### Question 4

Old environment's registers must be stored, so when scheduler decides to continue this environment, the environment is able to continue like nothing has happened.

The registers are saved in environment's trap frame.

### Exercise 7 

`sys_exofork()` fork an environment according to parent environment, but doesn't copy any memory mapping. As parent environment's status is saved in trap frame, I just make an copy of parent environment's trap frame for new environment. 
But `sys_exofork` returns different values to parent environment and child environment, child environment's id for the former and 0 for the latter. As child environment get the result from `$eax` but parent get it by return statement, just change child environment's trap frame's `reg_eax` to 0.

*One thing is not mentioned in both doc and comment, **env's break** should be copied.* 
*Creation of an env includes loading the instruction code and set the break pointer.*
*But `sys_exofork()` just call `env_alloc()` without the need to load instruction code. So copy the parent break pointer to child env.*

`sys_env_set_status()` sets target environment's status to `status`. Setting the third parameter to 1 to ensure current environment has the permission to manipulate the target environment.

`sys_page_alloc()` allocate a page at virtual address `va` in target environment's virtual memory space. Like `sys_env_set_status()`, current environment must have the permission to manipulate the target environment. 
For the permission on the new page, first check the `PTE_U` and `PTE_P` bit which is necessary and then zero the possible bit to validation.

`sys_page_map()` makes source environment and destination environment share the same physical page which `srcva` points to in source environment.

`sys_page_unmap()` unmaps `va` address to a physical address.

## Part B

Now the JOS allows user to create new environments by `fork()`. But for now before starting up the new environment, user must manually copy the parent's memory to the child. Cloning the memory is quite an expensive operation. 

Nowadays Unix take advantages of COW (copy-on-write) to make `fork()` more quickly. 
That is when fork, the memory of parent environment is marked read-only. Once either the child or parent tries to write, a page fault is triggered. Then kernel takes control and make a new copy of the read-only part, marks it as writable and replaces the mapping in the environment's page table. 
Then redo the operation triggered the page fault.
And the environment is able to continue.

This part is to implement a Unix like `fork()` with copy-on-write.

### Exercise 8

Handing the user level page fault to user-level page fault handler makes it more flexible and less bug demaging. That demands a user-level page fault handler registration in the target environment.

### Exercise 9

To dispatch page fault to user exception handler, we need to transfer necessary informations to user level trap frame. As user exception stack's space is quite limited, once run out, page faults and faults again.

One point to take notice, `page_fault_handler()` is run at kernel level. In that case directly calling user level exception handler is forbidden. 
To change the ring and dispatch, `page_fault_handler()` puts user exception handler entry and user exeption stack address into trap frame. `env_run()` changes the ring and enter the user exception handler.

### Exercise 10

As user trap frame show below, 

```C
struct UTrapframe {
	/* information about the fault */
	uint32_t utf_fault_va;	/* va for T_PGFLT, 0 otherwise */
	uint32_t utf_err;
	/* trap-time return state */
	struct PushRegs utf_regs;
	uintptr_t utf_eip;
	uint32_t utf_eflags;
	/* the trap-time stack to return to */
	uintptr_t utf_esp;
} __attribute__((packed));
```

`utf_esp` is above `utf_eip`. Before restoring registers, we must put `utf_eip` to another place. 
As `utf_esp` points to the stack bottom which trapped or last user level exception stack's bottom. It is better to put `utf_eip` right under `utf_esp` and decrease `utf_esp`.

### Exercise 11

User exception stack is only needed when user level exception handler is specified. Thus allocating page for user exception stack when specified. 
If change handler to another, no need to allocate again.

### Exercise 12

`fork()` function first sets parent process's page fault handler to handle page fault on copy-on-write pages. 
Then it creates the child environment. And copy memory address space to child environment. The space ranges from `UTEXT` to `USTACKTOP`. It is ok to traverse each page in the range and check the need to copy. But walk through the page directory and page tables saves some effort. 
User level exception stack is not shared between two envs, parent should prepare a different page as user exception stack for its child env.
As user stack is set as copy-on-write, once the child env starts to run, page fault on user stack. Thus parent env must prepare the page fault handler for child env. 
Now the child is ready to run.

`duppage()` copys parent memory space mapping to child's. If one page is writable, it should be set as copy-on-write both in parent's and child's. 
**One point is the sequence of map. Child env does first and parent does next.**
Reason: As for most pages, the wrong sequence doesn't make any difference. But for user stack, it does. 
First, we denote the current page where `%esp` resides as page A. 
Next we set page A as copy-on-write. When returning from `sys_page_map`, the stack changes leading to page fault. The page fault handler duplicates the current page and marks it as writable. We denotes the new page as page B. 
Then we call `sys_page_map` for child env. The child's user stack will be mapped to page B instead page A. Obviously this breaks down the memory space isolation between two envs. And page A make no sense to child env.

If the page is read-only, we just mapped it read-only in child env's.

`pgfault` first checks and panics if it is not triggered by writing to a copy-on-write page. We just duplicate the page and replace the old mapping. Then the program resumes.

### Challenge - sfork

`fork()` create a child env which is not allowed to modify parent env's memory space. `sfork()` create a shared one. This is some what useful when two envs communicates frequently.

The implementation of `sfork()` is quite similiar to `fork()`. The difference is that pages below user stack is shared between parent and child and child has the same permission as parent has. But user stack and user exception stack is not shared.

**Notice**
`thisenv` is defined in user level memory, as a global variable. To achieve proper functionality, we need to set `thisenv` to the real 'thisenv' `thisenv = &envs[ENVX(sys_getenvid())]`.

## Part C: Preemptive Multitasking and Inter-Process communication

So far the JOS can't gains the control of the CPU only when the program gives up voluntarily. 
This part first implements preemptive multitasking. Simply schedule the envs with round-robin policy when hardware clock interrupt occurs. That is, hardware clock interrupt must be enabled and we need to set the schedule routine.

In this part, envs on JOS communicate simply by IPC, mostly a model of shared memory.

### Exercise 13

Hardware clock interrupt is a interrupt request. For x86, we set a interrupt descriptor entry at `IRQ_OFFSET + IRQ_TIMER` in interrupt descriptor table.

`sti` enables `IF` flag in `EFLAGS`. As interrupt is disabled in kernel mode, when we run a user level env with `env_run()`, we should execute `sti`.

When we run `spin`, the program will trap with `trapno = 32 // Hardware clock`.

### Exercise 14

Once we trapped with a hardware clock interrupt, we call `sched_yield()` and gives up the control on CPU. Preemptive multitasking is roughly achieved.

One point is that acknowledge the interrupt before call `sched_yield()` by calling `lapic_eoi()`. As `sched_yield()` is a non-return function, we must clear the clock interrupt to let other env run.

### Exercise 15

The design of IPC in JOS is that receiver enters receiving status and yields and sender finds the receiver and hands data to receiver and wake up receiver.

The design allow two envs to transfer up to a 32 bit value and a page. The value will be stored in env's `env_ipc_value` while the page is reference by address in `env_ipc_dstva`.

As the kernel is protected by one lock, no race will occur for multiple sender to modify receiver env. 

Some times receiver don't need a page, or sender is not intended to send a page, we can set `pg` to `UTOP`. Though 0 is possily ok, but 0 is below `UTOP` and considered valid in some way. Thus `UTOP` is a better option.

As IPC happens whatever relation is between sender and receiver, 0 is the third parameter for `envid2env()` but `sys_page_map()` is not approriate to change receiver's memory address space.

`sys_ipc_recv()` calls `sched_yield()` right after setting up curenv. And `sched_yield()` doesn't return. So to return 0 must be done by `sys_ipc_try_send()` by specify the saved registers in receiver's trap frame to be 0.