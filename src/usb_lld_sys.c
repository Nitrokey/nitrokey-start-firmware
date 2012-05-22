#include "ch.h"
#include "hal.h"
#include "usb_lld.h"

CH_IRQ_HANDLER (Vector90) {
  CH_IRQ_PROLOGUE();
  chSysLockFromIsr();

  usb_interrupt_handler ();

  chSysUnlockFromIsr();
  CH_IRQ_EPILOGUE();
}

void usb_lld_sys_init (void)
{
  RCC->APB1ENR |= RCC_APB1ENR_USBEN;
  NVICEnableVector (USB_LP_CAN1_RX0_IRQn,
		    CORTEX_PRIORITY_MASK (STM32_USB_IRQ_PRIORITY));
  /*
   * Note that we also have other IRQ(s):
   * 	USB_HP_CAN1_TX_IRQn (for double-buffered or isochronous)
   * 	USBWakeUp_IRQn (suspend/resume)
   */
  RCC->APB1RSTR = RCC_APB1RSTR_USBRST;
  RCC->APB1RSTR = 0;

  USB_Cable_Config (1);
}

void usb_lld_sys_shutdown (void)
{
  RCC->APB1ENR &= ~RCC_APB1ENR_USBEN;
}
