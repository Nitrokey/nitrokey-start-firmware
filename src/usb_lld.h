/**
 * @brief USB interrupt priority level setting.
 */
#if !defined(STM32_USB_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define STM32_USB_IRQ_PRIORITY     11
#endif

void usb_lld_init (void);
