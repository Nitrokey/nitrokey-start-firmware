/* USB buffer memory definition and number of string descriptors */

#ifndef __USB_CONF_H
#define __USB_CONF_H

#define NUM_STRING_DESC 6

/* Control pipe */
/* EP0  */
#define ENDP0_RXADDR        (0x40)
#define ENDP0_TXADDR        (0x80)

/* CCID/ICCD BULK_IN, BULK_OUT */
/* EP1 */
#define ENDP1_TXADDR        (0xc0)
#define ENDP1_RXADDR        (0x100)

/* HID INTR_IN, INTR_OUT */
/* EP2 */
#define ENDP2_TXADDR        (0x140)
#define ENDP2_RXADDR        (0x148)

/* CDC BULK_IN, INTR_IN, BULK_OUT */
/* EP3  */
#define ENDP3_TXADDR        (0x14a)
/* EP4 */
#define ENDP4_TXADDR        (0x15a)
/* EP5 */
#define ENDP5_RXADDR        (0x162)

/* 0x172 - 0x180 : 14-byte */

/* MSC BULK_IN, BULK_OUT */
/* EP6 */
#define ENDP6_TXADDR        (0x180)
#define ENDP6_RXADDR        (0x1c0)

/* EP7: free */

#endif /* __USB_CONF_H */
