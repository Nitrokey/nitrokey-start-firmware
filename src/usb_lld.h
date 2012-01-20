#define STM32_USB_IRQ_PRIORITY     11
void usb_lld_init (void);

extern void usb_lld_to_pmabuf (const void *src, uint16_t addr, size_t n);
extern void usb_lld_from_pmabuf (void *dst, uint16_t addr, size_t n);

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

extern inline void usb_lld_txcpy (const void *src,
				  int ep_num, int offset, size_t len)
{
  usb_lld_to_pmabuf (src, GetEPTxAddr (ep_num) + offset, len);
}

extern inline void usb_lld_tx_enable (int ep_num, size_t len)
{
  SetEPTxCount (ep_num, len);
  SetEPTxValid (ep_num);
}

extern inline void usb_lld_write (uint8_t ep_num, const void *buf, size_t len)
{
  usb_lld_to_pmabuf (buf, GetEPTxAddr (ep_num), len);
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
  usb_lld_from_pmabuf (dst, GetEPRxAddr (ep_num) + offset, len);
}
