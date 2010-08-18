/*
(C) COPYRIGHT 2010 STMicroelectronics
usb_conf.h
MCD Application Team
V3.1.1
04/07/2010
Virtual COM Port Demo configuration  header
*/

#ifndef __USB_CONF_H
#define __USB_CONF_H

/*-------------------------------------------------------------*/
/* EP_NUM */
/* defines how many endpoints are used by the device */
/*-------------------------------------------------------------*/

#define EP_NUM                          (6)

/*-------------------------------------------------------------*/
/* --------------   Buffer Description Table  -----------------*/
/*-------------------------------------------------------------*/
/* buffer table base address */
/* buffer table base address */
#define BTABLE_ADDRESS      (0x00)

/* EP0  */
/* rx/tx buffer base address */
#define ENDP0_RXADDR        (0x40)
#define ENDP0_TXADDR        (0x80)

/* EP1  */
/* tx buffer base address */
#define ENDP1_TXADDR        (0xC0)
/* EP2 */
#define ENDP2_TXADDR        (0x100)
/* EP3 */
#define ENDP3_RXADDR        (0x110)


/* EP4 */
#define ENDP4_TXADDR        (0x180)
/* EP5 */
#define ENDP5_RXADDR        (0x1C0)

/*-------------------------------------------------------------*/
/* -------------------   ISTR events  -------------------------*/
/*-------------------------------------------------------------*/
/* IMR_MSK */
/* mask defining which events has to be handled */
/* by the device application software */
#define IMR_MSK (CNTR_CTRM  | CNTR_SOFM  | CNTR_RESETM )

/*#define CTR_CALLBACK*/
/*#define DOVR_CALLBACK*/
/*#define ERR_CALLBACK*/
/*#define WKUP_CALLBACK*/
/*#define SUSP_CALLBACK*/
/*#define RESET_CALLBACK*/
/*#define SOF_CALLBACK*/
/*#define ESOF_CALLBACK*/

#if 0
#define  EP1_IN_Callback   NOP_Process
#define  EP2_IN_Callback   NOP_Process
#define  EP3_IN_Callback   NOP_Process
#define  EP4_IN_Callback   NOP_Process
#define  EP5_IN_Callback   NOP_Process
#define  EP6_IN_Callback   NOP_Process
#define  EP7_IN_Callback   NOP_Process

#define  EP1_OUT_Callback   NOP_Process
#define  EP2_OUT_Callback   NOP_Process
#define  EP3_OUT_Callback   NOP_Process
#define  EP4_OUT_Callback   NOP_Process
#define  EP5_OUT_Callback   NOP_Process
#define  EP6_OUT_Callback   NOP_Process
#define  EP7_OUT_Callback   NOP_Process
#endif

#endif /* __USB_CONF_H */
