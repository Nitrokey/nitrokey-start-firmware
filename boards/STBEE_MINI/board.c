#include "config.h"
#include "ch.h"
#include "hal.h"

#include "../common/hwinit.c"

void
hwinit0 (void)
{
  hwinit0_common ();
}

void
hwinit1 (void)
{
  hwinit1_common ();

#if defined(PINPAD_SUPPORT)
  palWritePort(IOPORT2, 0x7fff); /* Only clear GPIOB_7SEG_DP */
  while (palReadPad (IOPORT2, GPIOB_BUTTON) != 0)
    ;				/* Wait for JTAG debugger connection */
  palWritePort(IOPORT2, 0xffff); /* All set */

  /* EXTI0 <= PB0 */
  AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI0_PB;
  EXTI->IMR = 0;
  EXTI->FTSR = EXTI_FTSR_TR0;
  NVICEnableVector(EXTI0_IRQn,
		   CORTEX_PRIORITY_MASK(CORTEX_MINIMUM_PRIORITY));

  /* TIM3 */
  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
  RCC->APB1RSTR = RCC_APB1RSTR_TIM3RST;
  RCC->APB1RSTR = 0;
  NVICEnableVector(TIM3_IRQn,
		   CORTEX_PRIORITY_MASK(CORTEX_MINIMUM_PRIORITY));
  TIM3->CR1 = TIM_CR1_URS | TIM_CR1_ARPE; /* Don't enable TIM3 for now */
  TIM3->CR2 = TIM_CR2_TI1S;
  TIM3->SMCR = TIM_SMCR_TS_0 | TIM_SMCR_TS_2 | TIM_SMCR_SMS_2;
  TIM3->DIER = 0;		/* Disable interrupt for now */
  TIM3->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_IC1F_0 | TIM_CCMR1_IC1F_3
    | TIM_CCMR1_CC2S_1 | TIM_CCMR1_IC2F_0 | TIM_CCMR1_IC2F_3;
  TIM3->CCMR2 = 0;
  TIM3->CCER =  TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC2P;
  TIM3->PSC = 72 - 1;		/* 1 MHz */
  TIM3->ARR = 18000;		/* 18 ms */
  /* Generate UEV to upload PSC and ARR */
  TIM3->EGR = TIM_EGR_UG;	
#endif
  /*
   * Disable JTAG and SWD, done after hwinit1_common as HAL resets AFIO
   */
  AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_DISABLE;
  /* We use LED2 as optional "error" indicator */
  palSetPad (IOPORT1, GPIOA_LED2);
}

void
USB_Cable_Config (FunctionalState NewState)
{
  if (NewState != DISABLE)
    palSetPad (IOPORT1, GPIOA_USB_ENABLE);
  else
    palClearPad (IOPORT1, GPIOA_USB_ENABLE);
}

void
set_led (int value)
{
  if (value)
    palClearPad (IOPORT1, GPIOA_LED1);
  else
    palSetPad (IOPORT1, GPIOA_LED1);
}

#if defined(PINPAD_SUPPORT)
void
cir_ext_disable (void)
{
  EXTI->PR = EXTI_PR_PR0;
  EXTI->IMR &= ~EXTI_IMR_MR0;
}

void
cir_ext_enable (void)
{
  EXTI->IMR |= EXTI_IMR_MR0;
}

extern void cir_ext_interrupt (void);
extern void cir_timer_interrupt (void);

CH_IRQ_HANDLER (EXTI0_IRQHandler)
{
  CH_IRQ_PROLOGUE ();
  chSysLockFromIsr ();

  cir_ext_interrupt ();

  chSysUnlockFromIsr ();
  CH_IRQ_EPILOGUE ();
}

CH_IRQ_HANDLER (TIM3_IRQHandler)
{
  CH_IRQ_PROLOGUE();
  chSysLockFromIsr();

  cir_timer_interrupt ();

  chSysUnlockFromIsr();
  CH_IRQ_EPILOGUE();
}
#endif
