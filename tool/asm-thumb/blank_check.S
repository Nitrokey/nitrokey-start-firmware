/* ARM Thumb Assembler code */
// arm-none-eabi-gcc -Wa,-amhls=blank_check.lst -c blank_check.S

	.cpu cortex-m3
	.thumb
	ldr	r1, .START_ADDR
	ldr	r2, .END_ADDR
0:	ldr	r0, [r1]
	add	r0, r0, #1
	bne	1f
	add	r1, r1, #4
	cmp	r1, r2
	bne	0b
1:	bkpt	#0x00
	// success: r0=0
	// fail:    r0!=0
	.align	2
.START_ADDR:	.word 0x08000000
.END_ADDR:	.word 0x08020000
