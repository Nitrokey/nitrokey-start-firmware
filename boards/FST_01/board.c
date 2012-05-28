#include "config.h"
#include "ch.h"
#include "hal.h"

#include "../common/hwinit.c"

void
hwinit1 (void)
{
  hwinit1_common ();

#if defined(PINPAD_CIR_SUPPORT)
  /* PA0/TIM2_CH1 = 1 (pull up)   */
  /* PA1/TIM2_CH2 = 0 (pull down) */
  /* PA2/TIM2_CH3 <= Vout of CIR receiver module */

  /* EXTI2 <= PA2 */
  AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI2_PA;
  EXTI->IMR = 0;
  EXTI->FTSR = EXTI_FTSR_TR2;
  NVICEnableVector(EXTI2_IRQn,
		   CORTEX_PRIORITY_MASK(CORTEX_MINIMUM_PRIORITY));
  /* TIM2 */
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  RCC->APB1RSTR = RCC_APB1RSTR_TIM2RST;
  RCC->APB1RSTR = 0;
  NVICEnableVector(TIM2_IRQn,
		   CORTEX_PRIORITY_MASK(CORTEX_MINIMUM_PRIORITY));

  TIM2->CR1 = TIM_CR1_URS | TIM_CR1_ARPE;
  TIM2->CR2 = TIM_CR2_TI1S;
  TIM2->SMCR = TIM_SMCR_TS_0 | TIM_SMCR_TS_2 | TIM_SMCR_SMS_2;
  TIM2->DIER = 0;		/* Disable interrupt for now */
  TIM2->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_IC1F_0 | TIM_CCMR1_IC1F_3
    | TIM_CCMR1_CC2S_1 | TIM_CCMR1_IC2F_0 | TIM_CCMR1_IC2F_3;
  TIM2->CCMR2 = 0;
  TIM2->CCER =  TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC2P;
  TIM2->PSC = 72 - 1;		/* 1 MHz */
  TIM2->ARR = 18000;		/* 18 ms */
  /* Generate UEV to upload PSC and ARR */
  TIM2->EGR = TIM_EGR_UG;	
#endif
}

void
USB_Cable_Config (int NewState)
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
    palSetPad (IOPORT2, GPIOB_LED);
  else
    palClearPad (IOPORT2, GPIOB_LED);
}

#if defined(PINPAD_CIR_SUPPORT)
void
cir_ext_disable (void)
{
  EXTI->PR = EXTI_PR_PR2;
  EXTI->IMR &= ~EXTI_IMR_MR2;
}

void
cir_ext_enable (void)
{
  EXTI->IMR |= EXTI_IMR_MR2;
}

extern void cir_ext_interrupt (void);
extern void cir_timer_interrupt (void);

CH_IRQ_HANDLER (EXTI2_IRQHandler)
{
  CH_IRQ_PROLOGUE ();
  chSysLockFromIsr ();

  cir_ext_interrupt ();

  chSysUnlockFromIsr ();
  CH_IRQ_EPILOGUE ();
}

CH_IRQ_HANDLER (TIM2_IRQHandler)
{
  CH_IRQ_PROLOGUE();
  chSysLockFromIsr();

  cir_timer_interrupt ();

  chSysUnlockFromIsr();
  CH_IRQ_EPILOGUE();
}
#endif
