#define STM32_USB_IRQ_PRIORITY     11
void usb_lld_init (void);

extern inline void usb_lld_stall_tx (int ep_num)
{
  SetEPTxStatus (ep_num, EP_TX_STALL);
}

extern inline void usb_lld_stall_rx (int ep_num)
{
  SetEPRxStatus (ep_num, EP_RX_STALL);
}

extern inline int usb_lld_tx_data_len (int ep_num)
{
  return GetEPTxCount (ep_num);
}

extern inline void usb_lld_txcpy (const uint8_t *src,
				  int ep_num, int offset, size_t len)
{
  UserToPMABufferCopy ((uint8_t *)src, GetEPTxAddr (ep_num) + offset, len);
}

extern inline void usb_lld_tx_enable (int ep_num, size_t len)
{
  SetEPTxCount (ep_num, len);
  SetEPTxValid (ep_num);
}

extern inline void usb_lld_write (uint8_t ep_num, void *buf, size_t len)
{
  UserToPMABufferCopy (buf, GetEPTxAddr (ep_num), len);
  SetEPTxCount (ep_num, len);
  SetEPTxValid (ep_num);
}

extern inline void usb_lld_rx_enable (int ep_num)
{
  SetEPRxValid (ep_num);
}

extern inline int usb_lld_rx_data_len (int ep_num)
{
  return GetEPRxCount (ep_num);
}

extern inline void usb_lld_rxcpy (uint8_t *dst,
				  int ep_num, int offset, size_t len)
{
  PMAToUserBufferCopy (dst, GetEPRxAddr (ep_num) + offset, len);
}
