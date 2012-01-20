#include "ch.h"
#include "hal.h"
#include "usb_lib.h"
#include "usb_lld.h"

extern void USB_Istr (void);

CH_IRQ_HANDLER (Vector90) {
  CH_IRQ_PROLOGUE();
  chSysLockFromIsr();

  USB_Istr();

  chSysUnlockFromIsr();
  CH_IRQ_EPILOGUE();
}

void usb_lld_init (void) {
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
}

void usb_lld_to_pmabuf (const void *src, uint16_t wPMABufAddr, size_t n)
{
  const uint8_t *s = (const uint8_t *)src;
  uint16_t *p;
  uint16_t w;

  if (n == 0)
    return;

  if ((wPMABufAddr & 1))
    {
      p = (uint16_t *)(PMAAddr + (wPMABufAddr - 1) * 2);
      w = *p;
      w = (w & 0xff) | (*s++) << 8;
      *p = w;
      p += 2;
      n--;
    }
  else
    p = (uint16_t *)(PMAAddr + wPMABufAddr * 2);

  while (n >= 2)
    {
      w = *s++;
      w |= (*s++) << 8;
      *p = w;
      p += 2;
      n -= 2;
    }

  if (n > 0)
    {
      w = *s;
      *p = w;
    }
}

void usb_lld_from_pmabuf (void *dst, uint16_t wPMABufAddr, size_t n)
{
  uint8_t *d = (uint8_t *)dst;
  uint16_t *p;
  uint16_t w;

  if (n == 0)
    return;

  if ((wPMABufAddr & 1))
    {
      p = (uint16_t *)(PMAAddr + (wPMABufAddr - 1) * 2);
      w = *p;
      *d++ = (w >> 8);
      p += 2;
      n--;
    }
  else
    p = (uint16_t *)(PMAAddr + wPMABufAddr * 2);

  while (n >= 2)
    {
      w = *p;
      *d++ = (w & 0xff);
      *d++ = (w >> 8);
      p += 2;
      n -= 2;
    }

  if (n > 0)
    {
      w = *p;
      *d = (w & 0xff);
    }
}
