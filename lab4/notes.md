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