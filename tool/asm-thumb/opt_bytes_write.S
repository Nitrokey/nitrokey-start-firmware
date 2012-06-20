/* ARM Thumb Assembler code */
// arm-none-eabi-gcc -Wa,-amhls=opt_bytes_write.lst -c opt_bytes_write.S

#define FLASH_CR_OPTPG	0x0010
#define FLASH_SR_BSY	0x0001

#define FLASH_SR_OFFSET	0x0c
#define FLASH_CR_OFFSET	0x10

#define OB_RDP_UNLOCK	0x00a5

	.cpu cortex-m3
	.thumb
	movw	r0, #OB_RDP_UNLOCK
	ldr	r1, .TARGET_ADDR
	ldr	r2, .FLASH_BASE_ADDR
	mov	r3, #FLASH_CR_OPTPG
	mov	r4, #FLASH_SR_BSY
	str	r3, [r2, #FLASH_CR_OFFSET]
	strh	r0, [r1]
1:	ldr	r0, [r2, #FLASH_SR_OFFSET]
	tst	r0, r4
	bne	1b
	mov	r0, #0
	str	r0, [r2, #FLASH_CR_OFFSET]
	bkpt	#0x00
	.align	2
.FLASH_BASE_ADDR:	.word 0x40022000
.TARGET_ADDR:		.word 0x1FFFF800
