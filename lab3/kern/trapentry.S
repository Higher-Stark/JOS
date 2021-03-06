/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
.globl _alltraps
TRAPHANDLER_NOEC(vector0, 0)
TRAPHANDLER_NOEC(vector1, 1)
TRAPHANDLER_NOEC(vector2, 2)
TRAPHANDLER_NOEC(vector3, 3)
TRAPHANDLER_NOEC(vector4, 4)
TRAPHANDLER_NOEC(vector5, 5)
TRAPHANDLER_NOEC(vector6, 6)
TRAPHANDLER_NOEC(vector7, 7)
TRAPHANDLER(vector8, 8)
TRAPHANDLER_NOEC(vector9, 9)
TRAPHANDLER(vector10, 10)
TRAPHANDLER(vector11, 11)
TRAPHANDLER(vector12, 12)
TRAPHANDLER(vector13, 13)
TRAPHANDLER(vector14, 14)
TRAPHANDLER_NOEC(vector15, 15)
TRAPHANDLER_NOEC(vector16, 16)
TRAPHANDLER(vector17, 17)
TRAPHANDLER_NOEC(vector18, 18)
TRAPHANDLER_NOEC(vector19, 19)
TRAPHANDLER_NOEC(vector20, 20)
TRAPHANDLER_NOEC(vector21, 21)
TRAPHANDLER_NOEC(vector22, 22)
TRAPHANDLER_NOEC(vector23, 23)
TRAPHANDLER_NOEC(vector24, 24)
TRAPHANDLER_NOEC(vector25, 25)
TRAPHANDLER_NOEC(vector26, 26)
TRAPHANDLER_NOEC(vector27, 27)
TRAPHANDLER_NOEC(vector28, 28)
TRAPHANDLER_NOEC(vector29, 29)
TRAPHANDLER_NOEC(vector30, 30)
TRAPHANDLER_NOEC(vector31, 31)
TRAPHANDLER_NOEC(vector32, 32)
TRAPHANDLER_NOEC(vector33, 33)
TRAPHANDLER_NOEC(vector34, 34)
TRAPHANDLER_NOEC(vector35, 35)
TRAPHANDLER_NOEC(vector36, 36)
TRAPHANDLER_NOEC(vector37, 37)
TRAPHANDLER_NOEC(vector38, 38)
TRAPHANDLER_NOEC(vector39, 39)
TRAPHANDLER_NOEC(vector40, 40)
TRAPHANDLER_NOEC(vector41, 41)
TRAPHANDLER_NOEC(vector42, 42)
TRAPHANDLER_NOEC(vector43, 43)
TRAPHANDLER_NOEC(vector44, 44)
TRAPHANDLER_NOEC(vector45, 45)
TRAPHANDLER_NOEC(vector46, 46)
TRAPHANDLER_NOEC(vector47, 47)
TRAPHANDLER_NOEC(vector48, 48)
TRAPHANDLER_NOEC(vector49, 49)
TRAPHANDLER_NOEC(vector50, 50)

/*
 * Lab 3: Your code here for _alltraps
 */
.text
.globl _alltraps
_alltraps:
	pushl %ds
	pushl %es 
	pushal

	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es

	pushl %esp
	call trap
	addl $4, %esp

  # Return falls through to trapret...
.globl trapret
trapret:
  popal
  popl %es
  popl %ds
  addl $0x8, %esp  # trapno and errcode
  iret

.text
.globl sysenter_handler
sysenter_handler:
	pushl %edi
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %eax
	call syscall
	movl %ebp, %ecx
	movl %esi, %edx
	sysexit
