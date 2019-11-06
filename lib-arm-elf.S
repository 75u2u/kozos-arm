/*
 * Use SWI instruction.
 * See gdb/sim/arm/armemu.c:ARMul_Emulate32(),armos.c:ARMul_OSHandleSWI()
 * (case 0xf0:)
 * See gdb/sim/testsuite/sim/arm/hello.ms
 */

#define SWI_Exit  0x11
#define SWI_Open  0x66
#define SWI_Close 0x68
#define SWI_Write 0x69
#define SWI_Read  0x6a

#define SYSCALL_BY_SWI
#ifdef SYSCALL_BY_SWI
#define SWI(arg) swi (arg)
#else
#define TOPBITS_AL (14 << 28) /* condition code */
#define SWI(arg) .long (TOPBITS_AL | (0xf0 << 20) | (arg))
#endif

	.section .text

	.globl	_start
	.type	_start, %function
_start:
	ldr	sp, _stack_addr
	bl	main

	/*
	 * Registers as return value and first argument are same.
	 * Setup of first argument is unneed.
	 */
	/* fall through */

	.globl	__exit
	.type	__exit, %function
__exit:
	SWI(SWI_Exit)
	mov	pc, lr

	.globl	__read
	.type	__read, %function
__read:
	SWI(SWI_Read)
	mov	pc, lr

	.globl	__write
	.type	__write, %function
__write:
	SWI(SWI_Write)
	mov	pc, lr

	.globl	__open
	.type	__open, %function
__open:
	/*
	 * Flags of open() exchanged by translate_open_mode[].
	 * See gdb/sim/arm/armos.c:SWIopen()
	 * Specify O_RDWR, O_CREAT, O_TRUNC flags.
	 * (See gdb/sim/arm/armos.c:translate_open_mode[])
	 */
	mov	r1, #6
	SWI(SWI_Open)
	mov	pc, lr

	.globl	__close
	.type	__close, %function
__close:
	SWI(SWI_Close)
	mov	pc, lr

	.align 4
_stack_addr:
	.long	_estack
