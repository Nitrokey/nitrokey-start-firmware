/* USB buffer memory definition and number of string descriptors */

#ifndef __USB_CONF_H
#define __USB_CONF_H

#define NUM_STRING_DESC 4

/* EP0  */
#define ENDP0_RXADDR        (0x40)
#define ENDP0_TXADDR        (0x80)

/* EP1 */
#define ENDP1_TXADDR        (0xc0)
/* EP2 */
#define ENDP2_RXADDR        (0x100)

/* EP3  */
#define ENDP3_TXADDR        (0x140)
/* EP4 */
#define ENDP4_TXADDR        (0x150)
/* EP5 */
#define ENDP5_RXADDR        (0x160)

/* EP6 */
#define ENDP6_TXADDR        (0x180)
/* EP7 */
#define ENDP7_RXADDR        (0x1c0)

#endif /* __USB_CONF_H */
