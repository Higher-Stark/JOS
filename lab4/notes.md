# Lab 4 notes

## Brief introduction

This lab makes JOS capable of running multiple CPUs and multitasking, though only SMP(symmetric multiprocessing) model supported.  

## Part 1

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

`sys_env_set_status()` sets target environment's status to `status`. Setting the third parameter to 1 to ensure current environment has the permission to manipulate the target environment.

`sys_page_alloc()` allocate a page at virtual address `va` in target environment's virtual memory space. Like `sys_env_set_status()`, current environment must have the permission to manipulate the target environment. 
For the permission on the new page, first check the `PTE_U` and `PTE_P` bit which is necessary and then zero the possible bit to validation.

`sys_page_map()` makes source environment and destination environment share the same physical page which `srcva` points to in source environment.

`sys_page_unmap()` unmaps `va` address to a physical address.
