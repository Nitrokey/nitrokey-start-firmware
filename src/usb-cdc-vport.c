/*
 * This file is included by usb_prop.c to provide Virtual COM port feature
 */

/* Original is ../Virtual_COM_Port/usb_prop.c by STMicroelectronics */
/* Chopped and modified for Gnuk */

#include "usb-cdc.h"

/* Original copyright notice is following: */
/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
* File Name          : usb_prop.c
* Author             : MCD Application Team
* Version            : V3.1.1
* Date               : 04/07/2010
* Description        : All processing related to Virtual Com Port Demo
********************************************************************************
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/

typedef struct
{
  uint32_t bitrate;
  uint8_t format;
  uint8_t paritytype;
  uint8_t datatype;
} LINE_CODING;

static LINE_CODING linecoding = {
  115200, /* baud rate*/
  0x00,   /* stop bits-1*/
  0x00,   /* parity - none*/
  0x08    /* no. of bits 8*/
};

static uint8_t *Virtual_Com_Port_GetLineCoding(uint16_t Length)
{
  if (Length == 0)
    {
      pInformation->Ctrl_Info.Usb_wLength = sizeof(linecoding);
      return NULL;
    }

  return (uint8_t *)&linecoding;
}

static uint8_t *Virtual_Com_Port_SetLineCoding(uint16_t Length)
{
  if (Length == 0)
    {
      pInformation->Ctrl_Info.Usb_wLength = sizeof(linecoding);
      return NULL;
    }

  return (uint8_t *)&linecoding;
}

static RESULT
Virtual_Com_Port_Data_Setup (uint8_t RequestNo)
{
  uint8_t    *(*CopyRoutine)(uint16_t);

  CopyRoutine = NULL;

  if (RequestNo == USB_CDC_REQ_GET_LINE_CODING)
    CopyRoutine = Virtual_Com_Port_GetLineCoding;
  else if (RequestNo == USB_CDC_REQ_SET_LINE_CODING)
    CopyRoutine = Virtual_Com_Port_SetLineCoding;

  if (CopyRoutine == NULL)
    return USB_UNSUPPORT;

  pInformation->Ctrl_Info.CopyData = CopyRoutine;
  pInformation->Ctrl_Info.Usb_wOffset = 0;
  (*CopyRoutine) (0);		/* Set Ctrl_Info.Usb_wLength */

  return USB_SUCCESS;
}

static RESULT
Virtual_Com_Port_NoData_Setup (uint8_t RequestNo)
{
  if (RequestNo == USB_CDC_REQ_SET_CONTROL_LINE_STATE)
    /* Do nothing and success  */
    return USB_SUCCESS;

  return USB_UNSUPPORT;
}
