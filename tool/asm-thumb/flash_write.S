/* ARM Thumb Assembler code */
// arm-none-eabi-gcc -Wa,-amhls=flash_write.lst -c flash_write.S

#define FLASH_CR_PG	0x0001 // == FLASH_SR_BSY
#define FLASH_CR_ERRORS	0x0014 // == PGERR | WRPRTERR
#define FLASH_SR_BSY	0x0001

#define FLASH_SR_OFFSET	0x0c
#define FLASH_CR_OFFSET	0x10

	.cpu cortex-m0
	.thumb
	ldr	r2, .SIZE
	ldr	r0, .SRC_ADDR
	ldr	r1, .TARGET_ADDR
	ldr	r4, .FLASH_BASE_ADDR
	mov	r5, #FLASH_CR_PG
	mov	r6, #FLASH_CR_ERRORS
	mov	r7, #0
	str	r5, [r4, #FLASH_CR_OFFSET]
0:	ldrh	r3, [r0, r7]
	strh	r3, [r1, r7]
1:	ldr	r3, [r4, #FLASH_SR_OFFSET]
	tst	r3, r5
	bne	1b
	tst	r3, r6
	bne	2f
	add	r7, r7, #0x02
	cmp	r7, r2
	bne	0b
2:	mov	r7, #0
	str	r7, [r4, #FLASH_CR_OFFSET]
	bkpt	#0x00
	.align	2
.FLASH_BASE_ADDR:	.word 0x40022000
.SRC_ADDR:		.word 0x2000003C
.TARGET_ADDR:		.word 0x08000000
.SIZE:			.word 0x00000000
