/*
(C) COPYRIGHT 2010 STMicroelectronics
usb_prop.h
MCD Application Team
V3.1.1
04/07/2010
All processing related to Virtual COM Port Demo (Endpoint 0)
*/

#ifndef __usb_prop_H
#define __usb_prop_H

#define SEND_ENCAPSULATED_COMMAND   0x00
#define GET_ENCAPSULATED_RESPONSE   0x01
#define SET_COMM_FEATURE            0x02
#define GET_COMM_FEATURE            0x03
#define CLEAR_COMM_FEATURE          0x04
#define SET_LINE_CODING             0x20
#define GET_LINE_CODING             0x21
#define SET_CONTROL_LINE_STATE      0x22
#define SEND_BREAK                  0x23

extern ONE_DESCRIPTOR Device_Descriptor;
extern ONE_DESCRIPTOR Config_Descriptor;
extern ONE_DESCRIPTOR String_Descriptor[4];
#endif /* __usb_prop_H */
