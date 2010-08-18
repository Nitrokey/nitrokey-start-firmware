#include "ch.h"
#include "hal.h"
#include "usb_lld.h"

extern void USB_Istr (void);

CH_IRQ_HANDLER (Vector90) {
  CH_IRQ_PROLOGUE();

  USB_Istr();

  CH_IRQ_EPILOGUE();
}

void usb_lld_init (void) {
  RCC->APB1ENR |= RCC_APB1ENR_USBEN;
  NVICEnableVector (USB_LP_CAN1_RX0_IRQn,
		    CORTEX_PRIORITY_MASK (STM32_USB_IRQ_PRIORITY));
  /*
   * Note that we also have other IRQs:
   * 	USB_HP_CAN1_TX_IRQn
   * 	USBWakeUp_IRQn
   */
  RCC->APB1RSTR = RCC_APB1RSTR_USBRST;
  RCC->APB1RSTR = 0;
}
