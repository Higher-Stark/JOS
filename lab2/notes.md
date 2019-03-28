# Lab 2 notes

## Part 2
### Exercise 2
#### Page translation

A linear address can be divided into 3 parts:

```
31             22 21              12 11           0
| Page Dir index | Page Table index | Page Offset |
```

Translating a linear address into a physical address, page directory index is used to find page table page.
Page table index helps find the page frame's physical address, and offset points out the corresponding physical address.

#### Page Protection

As page frame's address is aligned to 4K, the lower 12 bits of the address is zero. These 12 bits is used to indicates the page frame's states.

* Present bit   
    Whether the page frame is allocated. If P = 0, the remaining bits is invalid.
* Accessed and Dirty bits  
    Before a write to an address, the PTE's dirty is set.
* R/W bit (Read/Write bit) and User/Supervisor bit  
    These are not used for address translation. They are used for page protection.  
    U/S bit = 0: the page is for the operating system and other system software and related data.  
    U/S bit = 1: for application procedures and data.
    R/W bit = 0: read-only access
    R/W bit = 1: read/write access

The protection is provided by both page directory entry and page table entry.

From the table in Intel 80386 Manual, If the U/S bit is 0 in either page directory or page table entry, the page frame's U/S flag is 0, and R/W bit not checked. Otherwise, the page frame's U/S flag is 1. And the page frame is writable only when two R/W bit is 1.

#### Question 1

Variable x has the type of `uintptr_t`.

#### Page Table management

Function `boot_map_region_large` maps huge page into page directory, but the page directory entry's PS bit is not effective as PSE bit is not set in register cr4. Enable page size extension as soon as cr3 is changed.

### Part 3

#### Question 2

| Entry | Base Virtual Address | Points to (logically): |
| ----: | :------------------: | :--------------------: |
| 1023 | 0xffc00000 | Page table for top 4MB of phys memory |
| 1022 | 0xff800000 | Page table for second 4MB of physical memory |
| 1021 | 0xff400000 | Page table for third 4MB of physical memory |
| .. | .. | .. |
| 2 | 0x00800000 | |
| 1 | 0x00400000 | |
| 0 | 0x00000000 | |

#### Question 3

The PTE_U/PTE_S flag indicates whether user can access the page frame. Although in the same address space, the kernel memory is protected with the PTE_S set.

#### Question 4

The operating system can support up to 4G memory. Because the 2-level page table covers up to 4G memory space.

#### Question 5

Space overhead: 4K + 4K * 1024 = 5000K. 

Hugepage helps break down the overhead.

#### Question 6

```asm
	mov	$relocated, %eax
	jmp	*%eax           # <<<<------- after this point, EIP jumps above KERNBASE
relocated:
```

Because bofore change register `cr3`, the physical memory space [0 - 4MB] is mapped to [0 - 4MB] and [KERNBASE - KERNBASE + 4 MB] these tow virtual memory space. Though `cr3` changed and EIP is still low, instructions can still be fetched.

The transition is necessary so that user application's stack will not corrupt kernel's space if lower virtual memory space is mapped to kernel's space.

### Challenge

#### Allow user to reach 4GB memory space

* User/Kernel Mode transition

    因为用户可以访问4G的虚拟内存空间，每一个进程必须独占所有的内存空间，因此用户必须触发中断来切换至内核态。
    每一个进程都需要在内存空间中维护一个IDT，其中一个entry必须实现向内核态的转换。
    内核也需要一个属于自己的IDT在结束后返回到原来的进程。

    在应用和内核态切换时，需要把内存中原有的数据全部swap out，再行swap in。

* Physical Memory Access and I/O device access

    由于虚拟内存空间可以和物理内存实现1-1映射，内核访问内存就可以直接访问而不需要翻译。
    不过无论是内核态还是用户态，都需要在内存空间中维护一部分空间用于存放IDT，已分配页。
    I/O设备访问可以通过调用中断，向I/O设备进行输出。

* how the kernel would access a user environment's virtual address space

    用户态应用可以把需要传给内核的参数保存在一些寄存器中，然后内存被swap后可以将这些参数存到内存空间中。
    如果用户传递的是内存地址，可以在swap后再次swap in 用户内存，拷贝部分内存到内核态，在内核空间中纪录关联，并在返回后根据需要进行更新。

* Advantage

    让用户可以访问整个4G的内存空间能简化内存访问方式，不需要庞大的page table来做地址的翻译。

* Disadvantage

    效率低下，由于进程切换需要更换内存内容，反复的swap会带来高昂的overhead。
    每一时刻整个内存空间都由一个进程所独占，不同进程间共享内存实现困难。
