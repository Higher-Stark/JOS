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