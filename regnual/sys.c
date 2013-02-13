#include "types.h"

static void fatal (void)
{
  for (;;);
}

static void none (void)
{
}

/* Note: it is not reset */
static __attribute__ ((naked))
void entry (void)
{
  asm volatile ("ldr	r0, =__ram_end__\n\t"
		"ldr	r1, =__main_stack_size__\n\t"
		"sub	r0, r0, r1\n\t"
		"mov	sp, r0\n\t"
		"mov	r0, #0\n\t"
		"ldr	r1, =_bss_start\n\t"
		"ldr	r2, =_bss_end\n"
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
