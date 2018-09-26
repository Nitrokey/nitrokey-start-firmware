#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "board.h"
#include "mcu/stm32f103.h"

void
ackbtn_init (chopstx_intr_t *intr)
{
  chopstx_claim_irq (intr, INTR_REQ_EXTI);

  /* Configure EXTI line */
#ifdef AFIO_EXTICR_INDEX
  AFIO->EXTICR[AFIO_EXTICR_INDEX] = AFIO_EXTICR1_EXTIx_Py;
#endif
  EXTI->IMR &= ~EXTI_IMR;
  EXTI->RTSR |= EXTI_RTSR_TR;
}

void
ackbtn_enable (void)
{
  EXTI->PR |= EXTI_PR;
  EXTI->IMR |= EXTI_IMR;
}

void
ackbtn_disable (void)
{
  EXTI->IMR &= ~EXTI_IMR;
  EXTI->PR |= EXTI_PR;
}


