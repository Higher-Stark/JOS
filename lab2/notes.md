# Lab 2 notes

## Part 1

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

### Question

Variable x has the type of `uintptr_t`.

