/* USB buffer memory definition and number of string descriptors */

#ifndef __USB_CONF_H
#define __USB_CONF_H

#define NUM_STRING_DESC 7

/* Control pipe */
/* EP0: 64-byte, 64-byte  */
#define ENDP0_RXADDR        (0x40)
#define ENDP0_TXADDR        (0x80)

/* CCID/ICCD BULK_IN, BULK_OUT */
/* EP1: 64-byte, 64-byte */
#define ENDP1_TXADDR        (0xc0)
#define ENDP1_RXADDR        (0x100)
/* EP2: INTR_IN: 4-byte */
#define ENDP2_TXADDR        (0x140)

/* CDC BULK_IN, INTR_IN, BULK_OUT */
/* EP3: 16-byte  */
#define ENDP3_TXADDR        (0x144)
/* EP4: 8-byte */
#define ENDP4_TXADDR        (0x154)
/* EP5: 16-byte */
#define ENDP5_RXADDR        (0x15c)

/* 0x16c - 0x17e : 18-byte */

/* HID INTR_IN */
/* EP7: 2-byte */
#define ENDP7_TXADDR        (0x17e)

/* MSC BULK_IN, BULK_OUT */
/* EP6: 64-byte, 64-byte */
#define ENDP6_TXADDR        (0x180)
#define ENDP6_RXADDR        (0x1c0)

#endif /* __USB_CONF_H */
