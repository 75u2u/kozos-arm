#define SWI_Write 0x69
#define SWI_Read  0x6a

#define SYSCALL_BY_SWI
#ifdef SYSCALL_BY_SWI
#define SWI(arg) swi (arg)
#else
#define TOPBITS_AL (14 << 28) /* condition code */
#define SWI(arg) .long (TOPBITS_AL | (0xf0 << 20) | (arg))
#endif

	.arm
	.section .text
	.globl	_start
	.type	_start,%function
_start:
	ldr	sp, _stack_addr
	bl	main

	.globl	_read
	.type	_read, %function
_read:
	SWI(SWI_Read)
	mov	pc, lr

	.globl	_write
	.type	_write, %function
_write:
	SWI(SWI_Write)
	mov	pc, lr

	.align 4
_stack_addr:
	.long	intrstack
