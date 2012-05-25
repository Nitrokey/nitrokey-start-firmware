#include "types.h"
#include "sys.h"

#define STM32_SW_PLL		(2 << 0)
#define STM32_PLLSRC_HSE	(1 << 16)

#define STM32_PLLXTPRE_DIV1	(0 << 17)
#define STM32_PLLXTPRE_DIV2	(1 << 17)

#define STM32_HPRE_DIV1		(0 << 4)

#define STM32_PPRE1_DIV2	(4 << 8)

#define STM32_PPRE2_DIV2	(4 << 11)

#define STM32_ADCPRE_DIV4	(1 << 14)

#define STM32_MCO_NOCLOCK	(0 << 24)

#define STM32_SW		STM32_SW_PLL
#define STM32_PLLSRC		STM32_PLLSRC_HSE
#define STM32_HPRE		STM32_HPRE_DIV1
#define STM32_PPRE1		STM32_PPRE1_DIV2
#define STM32_PPRE2		STM32_PPRE2_DIV2
#define STM32_ADCPRE		STM32_ADCPRE_DIV4
#define STM32_MCO		STM32_MCO_NOCLOCK

#define STM32_PLLCLKIN		(STM32_HSECLK / 1)
#define STM32_PLLMUL		((STM32_PLLMUL_VALUE - 2) << 18)
#define STM32_PLLCLKOUT		(STM32_PLLCLKIN * STM32_PLLMUL_VALUE)
#define STM32_SYSCLK		STM32_PLLCLKOUT
#define STM32_HCLK		(STM32_SYSCLK / 1)

#define STM32_FLASHBITS		0x00000012

struct NVIC {
  uint32_t ISER[8];
  uint32_t unused1[24];
  uint32_t ICER[8];
  uint32_t unused2[24];
  uint32_t ISPR[8];
  uint32_t unused3[24];
  uint32_t ICPR[8];
  uint32_t unused4[24];
  uint32_t IABR[8];
  uint32_t unused5[56];
  uint32_t IPR[60];
};

#define NVICBase	((struct NVIC *)0xE000E100)
#define NVIC_ISER(n)	(NVICBase->ISER[n])
#define NVIC_ICPR(n)	(NVICBase->ICPR[n])
#define NVIC_IPR(n)	(NVICBase->IPR[n])

static void NVICEnableVector (uint32_t n, uint32_t prio)
{
  unsigned int sh = (n & 3) << 3;

  NVIC_IPR (n >> 2) = (NVIC_IPR(n >> 2) & ~(0xFF << sh)) | (prio << sh);
  NVIC_ICPR (n >> 5) = 1 << (n & 0x1F);
  NVIC_ISER (n >> 5) = 1 << (n & 0x1F);
}


#define PERIPH_BASE	0x40000000
#define APB2PERIPH_BASE	(PERIPH_BASE + 0x10000)
#define AHBPERIPH_BASE	(PERIPH_BASE + 0x20000)

struct RCC {
  __IO uint32_t CR;
  __IO uint32_t CFGR;
  __IO uint32_t CIR;
  __IO uint32_t APB2RSTR;
  __IO uint32_t APB1RSTR;
  __IO uint32_t AHBENR;
  __IO uint32_t APB2ENR;
  __IO uint32_t APB1ENR;
  __IO uint32_t BDCR;
  __IO uint32_t CSR;
};

#define RCC_BASE		(AHBPERIPH_BASE + 0x1000)
#define RCC			((struct RCC *)RCC_BASE)

#define RCC_APB1ENR_USBEN	0x00800000
#define RCC_APB1RSTR_USBRST	0x00800000

#define RCC_CR_HSION		0x00000001
#define RCC_CR_HSIRDY		0x00000002
#define RCC_CR_HSITRIM		0x000000F8
#define RCC_CR_HSEON		0x00010000
#define RCC_CR_HSERDY		0x00020000
#define RCC_CR_PLLON		0x01000000
#define RCC_CR_PLLRDY		0x02000000

#define RCC_CFGR_SWS		0x0000000C
#define RCC_CFGR_SWS_HSI	0x00000000

struct FLASH {
  __IO uint32_t ACR;
  __IO uint32_t KEYR;
  __IO uint32_t OPTKEYR;
  __IO uint32_t SR;
  __IO uint32_t CR;
  __IO uint32_t AR;
  __IO uint32_t RESERVED;
  __IO uint32_t OBR;
  __IO uint32_t WRPR;
};

#define FLASH_R_BASE	(AHBPERIPH_BASE + 0x2000)
#define FLASH		((struct FLASH *) FLASH_R_BASE)


#define RCC_APB2ENR_IOPAEN	0x00000004
#define RCC_APB2RSTR_IOPARST	0x00000004
#define RCC_APB2ENR_IOPDEN	0x00000020
#define RCC_APB2RSTR_IOPDRST	0x00000020

#define VAL_GPIOAODR            0xFFFFFFFF
#define VAL_GPIOACRH            0x88888888      /* PA15...PA8 */
#define VAL_GPIOACRL            0x88888884      /*  PA7...PA0 */

struct GPIO {
  __IO uint32_t CRL;
  __IO uint32_t CRH;
  __IO uint32_t IDR;
  __IO uint32_t ODR;
  __IO uint32_t BSRR;
  __IO uint32_t BRR;
  __IO uint32_t LCKR;
};

#define GPIOA_BASE	(APB2PERIPH_BASE + 0x0800)
#define GPIOA		((struct GPIO *) GPIOA_BASE)
#define GPIOB_BASE	(APB2PERIPH_BASE + 0x0C00)
#define GPIOB		((struct GPIO *) GPIOB_BASE)
#define GPIOC_BASE	(APB2PERIPH_BASE + 0x1000)
#define GPIOC		((struct GPIO *) GPIOC_BASE)
#define GPIOD_BASE	(APB2PERIPH_BASE + 0x1400)
#define GPIOD		((struct GPIO *) GPIOD_BASE)
#define GPIOE_BASE	(APB2PERIPH_BASE + 0x1800)
#define GPIOE		((struct GPIO *) GPIOE_BASE)

#ifdef GPIO_USB_BASE
#define GPIO_USB	((struct GPIO *) GPIO_USB_BASE)
#endif
#define GPIO_LED	((struct GPIO *) GPIO_LED_BASE)

static void
usb_cable_config (int on)
{
#ifdef GPIO_USB_BASE
# ifdef GPIO_USB_CLEAR_TO_ENABLE
  if (on)
    GPIO_USB->BRR = (1 << GPIO_USB_CLEAR_TO_ENABLE);
  else
    GPIO_USB->BSRR = (1 << GPIO_USB_CLEAR_TO_ENABLE);
# endif
# ifdef GPIO_USB_SET_TO_ENABLE
  if (on)
    GPIO_USB->BSRR = (1 << GPIO_USB_SET_TO_ENABLE);
  else
    GPIO_USB->BRR = (1 << GPIO_USB_SET_TO_ENABLE);
# endif
#else
  (void)on;
#endif
}

void
set_led (int on)
{
#ifdef GPIO_LED_CLEAR_TO_EMIT
  if (on)
    GPIO_LED->BRR = (1 << GPIO_LED_CLEAR_TO_EMIT);
  else
    GPIO_LED->BSRR = (1 << GPIO_LED_CLEAR_TO_EMIT);
#endif
#ifdef GPIO_LED_SET_TO_EMIT
  if (on)
    GPIO_LED->BSRR = (1 << GPIO_LED_SET_TO_EMIT);
  else
    GPIO_LED->BRR = (1 << GPIO_LED_SET_TO_EMIT);
#endif
}


#define USB_IRQ 20
#define USB_IRQ_PRIORITY ((11) << 4)

void usb_lld_sys_init (void)
{
  RCC->APB1ENR |= RCC_APB1ENR_USBEN;
  NVICEnableVector (USB_IRQ, USB_IRQ_PRIORITY);
  RCC->APB1RSTR = RCC_APB1RSTR_USBRST;
  RCC->APB1RSTR = 0;
  usb_cable_config (1);
}

void usb_lld_sys_shutdown (void)
{
  RCC->APB1ENR &= ~RCC_APB1ENR_USBEN;
}


#define FLASH_KEY1               0x45670123UL
#define FLASH_KEY2               0xCDEF89ABUL

enum flash_status
{
  FLASH_BUSY = 1,
  FLASH_ERROR_PG,
  FLASH_ERROR_WRP,
  FLASH_COMPLETE,
  FLASH_TIMEOUT
};

#define FLASH_SR_BSY		0x01
#define FLASH_SR_PGERR		0x04
#define FLASH_SR_WRPRTERR	0x10
#define FLASH_SR_EOP		0x20

#define FLASH_CR_PG	0x0001
#define FLASH_CR_PER	0x0002
#define FLASH_CR_MER	0x0004
#define FLASH_CR_OPTPG	0x0010
#define FLASH_CR_OPTER	0x0020
#define FLASH_CR_STRT	0x0040
#define FLASH_CR_LOCK	0x0080
#define FLASH_CR_OPTWRE	0x0200
#define FLASH_CR_ERRIE	0x0400
#define FLASH_CR_EOPIE	0x1000

#define FLASH_OBR_RDPRT 0x00000002

#define OPTION_BYTES_ADDR 0x1ffff800

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
  asm volatile ("cpsid	i\n\t"	/* Mask all interrupts */
		"ldr	r0, =__ram_end__\n\t"
		"msr	MSP, r0\n\t" /* Main (interrupt handler) stack */
		"ldr	r1, =__main_stack_size__\n\t"
		"subs	r0, r0, r1\n\t"
		"msr	PSP, r0\n\t" /* Process (main routine) stack */
		"movs	r0, #0\n\t"
		"ldr	r1, =_bss_start\n\t"
		"ldr	r2, =_bss_start\n"
	"0:\n\t"
		"cmp	r1, r2\n\t"
		"bge	1f\n\t"
		"str	r0, [r1]\n\t"
		"adds	r1, r1, #4\n\t"
		"b	0b\n"
	"1:\n\t"
		"movs	r0, #2\n\t" /* Switch to PSP */
		"msr	CONTROL, r0\n\t"
		"isb\n\t"
		"movs	r0, #0\n\t"
		"msr	BASEPRI, r0\n\t" /* Enable interrupts */
		"cpsie	i\n\t"
		"mov	r1, r0\n\t"
		"bl	main\n"
	"2:\n\t"
		"b	2b\n"
		: /* no output */ : /* no input */ : "memory");
}

#define intr_disable()  asm volatile ("cpsid   i" : : "r" (0) : "memory")

#define intr_enable()  asm volatile ("msr     BASEPRI, %0\n\t"		 \
				     "cpsie   i" : : "r" (0) : "memory")

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

static int
flash_get_status (void)
{
  int status;

  if ((FLASH->SR & FLASH_SR_BSY) != 0)
    status = FLASH_BUSY;
  else if ((FLASH->SR & FLASH_SR_PGERR) != 0)
    status = FLASH_ERROR_PG;
  else if((FLASH->SR & FLASH_SR_WRPRTERR) != 0 )
    status = FLASH_ERROR_WRP;
  else
    status = FLASH_COMPLETE;

  return status;
}

static int
flash_wait_for_last_operation (uint32_t timeout)
{
  int status;

  do
    if (--timeout == 0)
      return FLASH_TIMEOUT;
    else
      status = flash_get_status ();
  while (status == FLASH_BUSY);

  return status;
}

#define FLASH_PROGRAM_TIMEOUT 0x00010000
#define FLASH_ERASE_TIMEOUT   0x01000000

static int
flash_program_halfword (uint32_t addr, uint16_t data)
{
  int status;

  status = flash_wait_for_last_operation (FLASH_PROGRAM_TIMEOUT);

  intr_disable ();
  if (status == FLASH_COMPLETE)
    {
      FLASH->CR |= FLASH_CR_PG;

      *(volatile uint16_t *)addr = data;

      status = flash_wait_for_last_operation (FLASH_PROGRAM_TIMEOUT);
      FLASH->CR &= ~FLASH_CR_PG;
    }
  intr_enable ();

  return status;
}

int
flash_write (uint32_t dst_addr, const uint8_t *src, size_t len)
{
  int status;

  while (len)
    {
      uint16_t hw = *src++;

      hw |= (*src++ << 8);
      status = flash_program_halfword (dst_addr, hw);
      if (status != FLASH_COMPLETE)
	return 0;		/* error return */

      dst_addr += 2;
      len -= 2;
    }

  return 1;
}

int
flash_protect (void)
{
  int status;
  uint32_t option_bytes_value;

  status = flash_wait_for_last_operation (FLASH_ERASE_TIMEOUT);

  intr_disable ();
  if (status == FLASH_COMPLETE)
    {
      FLASH->OPTKEYR = FLASH_KEY1;
      FLASH->OPTKEYR = FLASH_KEY2;

      FLASH->CR |= FLASH_CR_OPTER;
      FLASH->CR |= FLASH_CR_STRT;

      status = flash_wait_for_last_operation (FLASH_ERASE_TIMEOUT);
      FLASH->CR &= ~FLASH_CR_OPTER;
    }
  intr_enable ();

  if (status != FLASH_COMPLETE)
    return 0;

  option_bytes_value = *(uint32_t *)OPTION_BYTES_ADDR;
  return (option_bytes_value & 0xff) == 0xff ? 1 : 0;
}

struct SCB
{
  __IO uint32_t CPUID;
  __IO uint32_t ICSR;
  __IO uint32_t VTOR;
  __IO uint32_t AIRCR;
  __IO uint32_t SCR;
  __IO uint32_t CCR;
  __IO uint8_t  SHP[12];
  __IO uint32_t SHCSR;
  __IO uint32_t CFSR;
  __IO uint32_t HFSR;
  __IO uint32_t DFSR;
  __IO uint32_t MMFAR;
  __IO uint32_t BFAR;
  __IO uint32_t AFSR;
  __IO uint32_t PFR[2];
  __IO uint32_t DFR;
  __IO uint32_t ADR;
  __IO uint32_t MMFR[4];
  __IO uint32_t ISAR[5];
};

#define SCS_BASE	(0xE000E000)
#define SCB_BASE	(SCS_BASE +  0x0D00)
#define SCB		((struct SCB *) SCB_BASE)

#define SYSRESETREQ 0x04
void nvic_system_reset (void)
{
  SCB->AIRCR = (0x05FA0000 | (SCB->AIRCR & 0x70) | SYSRESETREQ);
  asm volatile ("dsb");
}
