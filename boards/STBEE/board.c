#include "config.h"
#include "ch.h"
#include "hal.h"

/*
 * Board-specific initialization code.
 */
void boardInit(void)
{
#if defined(PINPAD_CIR_SUPPORT)
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
#elif defined(PINPAD_DIAL_SUPPORT)
  /* EXTI2 <= PB2 */
  AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI2_PB;
  EXTI->IMR = 0;
  EXTI->FTSR = EXTI_FTSR_TR2;
  NVICEnableVector(EXTI2_IRQn,
		   CORTEX_PRIORITY_MASK(CORTEX_MINIMUM_PRIORITY));

  /* TIM4 */
  RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
  RCC->APB1RSTR = RCC_APB1RSTR_TIM4RST;
  RCC->APB1RSTR = 0;

  TIM4->CR1 = TIM_CR1_URS |  TIM_CR1_ARPE | TIM_CR1_CKD_1;
  TIM4->CR2 = 0;
  TIM4->SMCR = TIM_SMCR_SMS_0;
  TIM4->DIER = 0;		/* no interrupt */
  TIM4->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0
    | TIM_CCMR1_IC1F_0 | TIM_CCMR1_IC1F_1 | TIM_CCMR1_IC1F_2 | TIM_CCMR1_IC1F_3
    | TIM_CCMR1_IC2F_0 | TIM_CCMR1_IC2F_1 | TIM_CCMR1_IC2F_2 | TIM_CCMR1_IC2F_3;
  TIM4->CCMR2 = 0;
  TIM4->CCER = 0;
  TIM4->PSC = 0;
  TIM4->ARR = 31;
  /* Generate UEV to upload PSC and ARR	 */
  TIM4->EGR = TIM_EGR_UG;
#endif
}

#if defined(PINPAD_CIR_SUPPORT)
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
#elif defined(PINPAD_DIAL_SUPPORT)
void
dial_sw_disable (void)
{
  EXTI->PR = EXTI_PR_PR2;
  EXTI->IMR &= ~EXTI_IMR_MR2;
}

void
dial_sw_enable (void)
{
  EXTI->IMR |= EXTI_IMR_MR2;
}

extern void dial_sw_interrupt (void);

CH_IRQ_HANDLER (EXTI2_IRQHandler)
{
  CH_IRQ_PROLOGUE ();
  chSysLockFromIsr ();

  dial_sw_interrupt ();

  chSysUnlockFromIsr ();
  CH_IRQ_EPILOGUE ();
}
#endif
