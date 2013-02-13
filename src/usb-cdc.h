/*
 * Class specific control requests for CDC
 */
#define USB_CDC_REQ_SET_LINE_CODING             0x20
#define USB_CDC_REQ_GET_LINE_CODING             0x21
#define USB_CDC_REQ_SET_CONTROL_LINE_STATE      0x22
#define USB_CDC_REQ_SEND_BREAK                  0x23

#define VIRTUAL_COM_PORT_DATA_SIZE              16
#define VIRTUAL_COM_PORT_INT_SIZE               8
