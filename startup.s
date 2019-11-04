	.arm
	.section .text
	.global	start
#	.type	start,@function
start:
#	mov	#bootstack,sp
	bl	main
#	nop
#1:
#        b 1b
	b .
        .global _dispatch
#       .type   _dispatch,@function
_dispatch:
