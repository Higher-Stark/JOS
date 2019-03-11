# JOS Lab 1 notes

## Part 1 - PC Bootstrap

### Exercise 1

#### AT&T Assembly Syntax

- Registers is prefixed with '%'

- Source/Destination Order

  source is on the *left*
  destination is on the *right*

- Constant and Immediate value is prefixed with a '$'

- Instruction is suffixed with 'b', 'w', 'l' to specify the width of the destination register

### Exercise 2

* `[f000:fff0]  ljmp $0xf0000, $0xe05b` instruction jumps to memory address 0xfe05b, where BIOS ROM resides.

## Part 2 - The Boot Loader

### Exercise 3

1. Once enter protected mode, processor starts to execute 32-bit code. When virtual address flag is on, 16-bit is shifted to 32-bit

2. The last instruction boot loader executes is `call *0x10018`, the first instruction kernel just loaded is `add    0x1bad(%eax),%dh`.

3. `ELFHDR->e_phnum` tells how much sector to load.

### Exercise 5

* Entering boot loader

```
(gdb) x/8x 0x100000
0x100000:   0x00000000  0x00000000  0x00000000  0x00000000
0x100010:   0x00000000  0x00000000  0x00000000  0x00000000
```

* Enter kernel

```gdb
(gdb) x/8x 0x100000
0x100000:       0x1badb002      0x00000000      0xe4524ffe      0x7205c766
0x100010:       0x34000004      0x0000b812      0x220f0011      0xc0200fd8
```

__Why different?__

Boot loader loads kernel codes to physical memory address 0x100000.

## Part 3 - Kernel

### Exercise 7

As virtual address ranging 0xf0000000 through 0xf0400000 is mapped to 0x000000 through 0x400000, `jmp *%eax` works. Otherwise, the jump will cause hardware exception in old map. 

Address from 0x000000 to 0x400000 is mapped to 0x000000 through 0x400000. Thus program is able to execute the next instruction.

### Question

1. Interface between console.c and printf.c

    console.c export function `void cputchar(int c)` as the interface, printf.c wraps this interface up in the function void putch(int ch, int *cnt).

2.  Code in console.c

      ```C
      if (crt_pos >= CRT_SIZE) {
      	int i;

      	memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
      	for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
      		crt_buf[i] = 0x0700 | ' ';
      	crt_pos -= CRT_COLS;
      }
      ```

    The code snippets handles the case when the CGA buffer is full. That is, top line is removed.

3. Execution of following code

    ```C
    int x = 1, y = 3, z = 4;
    cprintf("x %d, y %x, z %d\n", x, y, z);
    ```

    ```C
    vcprintf("x %d, y %x, z %d\n", 1, 3, 4)
    cons_putc(0xf0101968)
    cons_putc(0xf01019b9)
    va_arg()
      old ap: 0xf010feb4(\001)
      new ap: 0ff010feb8(\003)
    cons_putc(0xf01019ba)
    cons_putc(0xf01009b7)
    cons_putc(0xf01019bc)
    cons_putc(0xf01019bd)
    cons_putc(0xf01019be)
    cons_putc(0xf01019bf)
    va_arg()
      old ap: 0xf010feb8(\003)
      new ap: 0ff010febc(\004)
    cons_putc(0xf01009b7)
    cons_putc(0xf01019c2)
    cons_putc(0xf01019c3)
    cons_putc(0xf01019c4)
    cons_putc(0xf01019c5)
    va_arg()
      old ap: 0xf010febc(\004)
      new ap: 0ff010fec0("")
    cons_putc(0xf01019c6)
    cons_putc(0xf01009b7)
    cons_putc(0xf01019c8)
    ```

4. Execution of the following code
   
     ```C
        unsigned int i = 0x00646c72;
         cprintf("H%x Wo%s", 57616, &i);
     ```

    The output is `He110 World`.

    The function `cprintf` will first print character 'H', then print the heximal form of integer 57616, which is `0xe110`. " Wo" will be printed afterwards. At last, integer i is printed in string format. The layout of 0x00646c72 in memory is 0x72 0x6c 0x64 0x00, as it is on a little-endian machine. The first three bytes is mapped to 'r', 'l', 'd' respectively. 

    Otherwise, if on a big-endian machine, i is expected to be 0x726c6400 to yield the same output. But 57616 is not affected.

5. Number after y is a random value, which is the next four bytes value following 3. These four bytes are not set intentionally when referenced.

6. Add an integer argument in the parameter list, indicating the number of arguments followed. This integer is better to place just before the `...`

### The Stack

#### Exercise 12

Kernel initializes the stack at the 77 line in file entry.S, `movl	$(bootstacktop),%esp`. The stack's top is located at 0xf0108000 to 0xf0110000. The stack space is reserved as the data segment. As stack grows downwards, the stack pointer is initialized to point to virtual address 0xf0110000, same as instruction `movl 0xf0110000, %esp` told.

#### Exercise 13

Test_backtrace push 32 bytes onto the stack each time. The 32 bytes are return address, saved $ebp and arguments(such as 0xf01009a4 is pointed to putch).

#### Exercise 15

Dissambling obj/kern/kernel, the .stab contains debug info for the entire executable file. 
The general of the stab is a list of 'file', whose first line indicates the file type and the index in str table, variables and statements following the first line.

As these 'files' are arranged by the lowest instruction address, to find a line where it is, just search this sorted array by instruction address. First by file, set `n_type` to `N_SO`. Second by function, set `n_type` to `N_FUN`. Third by instruction address, set `n_type` to `SLINE`. If not found, then the instruction is probably from a function which is inlined. Thus find previous  with `n_type` equals `N_SOL`.

  >*n_desc* field indicates the line instruction lies in the source file.  
  >*n_value* field indicates the address of the assembly.
  
#### Exercise 16

As `%n` writes to one byte memory with the pointer given. Get `%ebp` with function `read_pretaddr`, and pass it to `cprintf` function, we can modify return address and call `do_overflow` function.

#### Exercise 17

Function `read_tsc` in x86.h has wraps `rdtsc` assembly up, make it easy to implement `time` command.

Indeed, there are a lot of concern to use `rdtsc` from Intel Mannual as following.

* Avoid out-of-order execution, which can be achieved by `cpuid` assembly.
* Disable interrupt.

These risks are not taken care of in this lab.