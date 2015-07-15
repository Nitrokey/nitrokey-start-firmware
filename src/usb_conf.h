/* USB buffer memory definition and number of string descriptors */

#ifndef __USB_CONF_H
#define __USB_CONF_H

#define ICC_NUM_INTERFACES 1
#define ICC_INTERFACE 0
#ifdef HID_CARD_CHANGE_SUPPORT
#define HID_NUM_INTERFACES 1
#define HID_INTERFACE 1
#else
#define HID_NUM_INTERFACES 0
#endif
#ifdef ENABLE_VIRTUAL_COM_PORT
#define VCOM_NUM_INTERFACES 2
#define VCOM_INTERFACE_0 (ICC_NUM_INTERFACES + HID_NUM_INTERFACES)
#define VCOM_INTERFACE_1 (ICC_NUM_INTERFACES + HID_NUM_INTERFACES + 1)
#else
#define VCOM_NUM_INTERFACES 0
#endif
#ifdef PINPAD_DND_SUPPORT
#define MSC_NUM_INTERFACES 1
#define MSC_INTERFACE (ICC_NUM_INTERFACES + HID_NUM_INTERFACES + VCOM_NUM_INTERFACES)
#else
#define MSC_NUM_INTERFACES 0
#endif
#define NUM_INTERFACES (ICC_NUM_INTERFACES + HID_NUM_INTERFACES \
			+ VCOM_NUM_INTERFACES + MSC_NUM_INTERFACES)

#if defined(USB_SELF_POWERED)
#define USB_INITIAL_FEATURE 0xC0   /* bmAttributes: self powered */
#else
#define USB_INITIAL_FEATURE 0x80   /* bmAttributes: bus powered */
#endif

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
