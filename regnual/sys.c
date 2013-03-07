#include "types.h"

static void fatal (void)
{
  for (;;);
}

static void none (void)
{
}

/*
 * Note: the address of this routine 'entry' will be in the vectors as
 * RESET, but this will be called from application.  It's not RESET
 * state, then.
 */
static __attribute__ ((naked,section(".text.entry")))
void entry (void)
{
  asm volatile ("mov	r0, pc\n\t"
		"bic	r0, r0, #255\n\t" /* R0 := vector_table address */
		"mov	r1, #0x90\n"	  /* R1 := numbers of entries * 4 */
	"0:\n\t"
		"ldr	r2, [r0, r1]\n\t"
		"add	r2, r2, #-0x20000000\n\t"
		"sub	r2, r2, #0x1400\n\t"
		"add	r2, r2, r0\n\t" /* Relocate: -0x20001400 + R0 */
		"str	r2, [r0, r1]\n\t"
		"subs	r1, r1, #4\n\t"
		"bne	0b\n\t"
		/* Relocation done.  We don't care the first entry.  */
		"ldr	r3, .L00\n"
	".LPIC00:\n\t"
		"add	r3, pc\n\t" /* R3 := @_GLOBAL_OFFSET_TABLE_ */
		"ldr	r4, .L00+4\n\t"
		"ldr	r0, [r3, r4]\n\t"
		"ldr	r4, .L00+8\n\t"
		"ldr	r1, [r3, r4]\n\t"
		"sub	r0, r0, r1\n\t"
		"mov	sp, r0\n\t"
		/* Clear BSS.  */
		"mov	r0, #0\n\t"
		"ldr	r4, .L00+12\n\t"
		"ldr	r1, [r3, r4]\n\t"
		"ldr	r4, .L00+16\n\t"
		"ldr	r2, [r3, r4]\n"
	"0:\n\t"
		"str	r0, [r1], #4\n\t"
		"cmp	r2, r1\n\t"
		"bhi	0b\n\t"
		"cpsie	i\n\t"	/* Enable interrupts */
		"mov	r0, #0\n\t"
		"mov	r1, r0\n\t"
		"bl	main\n"
	"1:\n\t"
		"b	1b\n"
	".L00:\n\t"
		".word	_GLOBAL_OFFSET_TABLE_-(.LPIC00+4)\n\t"
		".word	__ram_end__(GOT)\n\t"
		".word	__main_stack_size__(GOT)\n\t"
		".word	_bss_start(GOT)\n\t"
		".word	_bss_end(GOT)"
		: /* no output */ : /* no input */ : "memory");
}

typedef void (*handler)(void);
extern uint8_t __ram_end__;
extern void usb_interrupt_handler (void);

handler vector_table[] __attribute__ ((section(".vectors"))) = {
  (handler)&__ram_end__,
  entry,
  fatal,		/* nmi */
  fatal,		/* hard fault */
  /* 10 */
  fatal,		/* mem manage */
  fatal,		/* bus fault */
  fatal,		/* usage fault */
  none,
  /* 20 */
  none, none, none, none,  none, none, none, none,
  /* 40 */
  none, none, none, none,  none, none, none, none,
  /* 60 */
  none, none, none, none,  none, none, none, none,
  /* 80 */
  none, none, none, none,
  /* 90 */
  usb_interrupt_handler,
};
