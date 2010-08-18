/*
usb_prop.c
MCD Application Team
V3.1.1
04/07/2010
All processing related to Virtual Com Port Demo
*/

#include "usb_lib.h"
#include "usb_conf.h"
#include "usb_prop.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#include "hw_config.h"

#if 0
static uint8_t Request = 0;
#endif

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


#define GNUK_MAX_PACKET_SIZE 64

static void
gnuk_device_init (void)
{

  /* Update the serial number string descriptor with the data from the unique
  ID*/
  Get_SerialNum ();

  pInformation->Current_Configuration = 0;

  /* Connect the device */
  PowerOn ();

  /* Perform basic device initialization operations */
  USB_SIL_Init ();

  bDeviceState = UNCONNECTED;
}

static void
gnuk_device_reset (void)
{
  /* Set Virtual_Com_Port DEVICE as not configured */
  pInformation->Current_Configuration = 0;

  /* Current Feature initialization */
  pInformation->Current_Feature = Config_Descriptor.Descriptor[7];

  /* Set Virtual_Com_Port DEVICE with the default Interface*/
  pInformation->Current_Interface = 0;

  SetBTABLE (BTABLE_ADDRESS);

  /* Initialize Endpoint 0 */
  SetEPType (ENDP0, EP_CONTROL);
  SetEPTxStatus (ENDP0, EP_TX_STALL);
  SetEPRxAddr (ENDP0, ENDP0_RXADDR);
  SetEPTxAddr (ENDP0, ENDP0_TXADDR);
  Clear_Status_Out (ENDP0);
  SetEPRxCount (ENDP0, GNUK_MAX_PACKET_SIZE);
  SetEPRxValid (ENDP0);

  /* Initialize Endpoint 1 */
  SetEPType (ENDP1, EP_BULK);
  SetEPTxAddr (ENDP1, ENDP1_TXADDR);
  SetEPTxStatus (ENDP1, EP_TX_NAK);
  SetEPRxStatus (ENDP1, EP_RX_DIS);

  /* Initialize Endpoint 2 */
  SetEPType (ENDP2, EP_INTERRUPT);
  SetEPTxAddr (ENDP2, ENDP2_TXADDR);
  SetEPRxStatus (ENDP2, EP_RX_DIS);
  SetEPTxStatus (ENDP2, EP_TX_NAK);

  /* Initialize Endpoint 3 */
  SetEPType (ENDP3, EP_BULK);
  SetEPRxAddr (ENDP3, ENDP3_RXADDR);
  SetEPRxCount (ENDP3, VIRTUAL_COM_PORT_DATA_SIZE);
  SetEPRxStatus (ENDP3, EP_RX_VALID);
  SetEPTxStatus (ENDP3, EP_TX_DIS);

  /* Initialize Endpoint 4 */
  SetEPType (ENDP4, EP_BULK);
  SetEPTxAddr (ENDP4, ENDP4_TXADDR);
  SetEPTxStatus (ENDP4, EP_TX_NAK);
  SetEPRxStatus (ENDP4, EP_RX_DIS);

  /* Initialize Endpoint 5 */
  SetEPType (ENDP5, EP_BULK);
  SetEPRxAddr (ENDP5, ENDP5_RXADDR);
  SetEPRxCount (ENDP5, VIRTUAL_COM_PORT_DATA_SIZE); /* XXX */
  SetEPRxStatus (ENDP5, EP_RX_VALID);
  SetEPTxStatus (ENDP5, EP_TX_DIS);

  /* Set this device to response on default address */
  SetDeviceAddress (0);

  bDeviceState = ATTACHED;
}

static void
gnuk_device_SetConfiguration (void)
{
  DEVICE_INFO *pInfo = &Device_Info;

  if (pInfo->Current_Configuration != 0)
    /* Device configured */
    bDeviceState = CONFIGURED;
}

static void
gnuk_device_SetDeviceAddress (void)
{
  bDeviceState = ADDRESSED;
}

/* IN from port 0 */
static void
gnuk_device_Status_In (void)
{
#if 0
  if (Request == SET_LINE_CODING)
    Request = 0;
#endif
}

/* OUT to port 0 */
static void
gnuk_device_Status_Out (void)
{
}

/*******************************************************************************
* Function Name  : Virtual_Com_Port_Data_Setup
* Description    : handle the data class specific requests
* Input          : Request Nb.
* Output         : None.
* Return         : USB_UNSUPPORT or USB_SUCCESS.
*******************************************************************************/
static RESULT
Virtual_Com_Port_Data_Setup (uint8_t RequestNo)
{
  uint8_t    *(*CopyRoutine)(uint16_t);

  CopyRoutine = NULL;

  if (RequestNo == GET_LINE_CODING)
    {
      if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
	CopyRoutine = Virtual_Com_Port_GetLineCoding;
    }
  else if (RequestNo == SET_LINE_CODING)
    {
      if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
	CopyRoutine = Virtual_Com_Port_SetLineCoding;
#if 0
      Request = SET_LINE_CODING;
#endif
    }

  if (CopyRoutine == NULL)
    return USB_UNSUPPORT;

  pInformation->Ctrl_Info.CopyData = CopyRoutine;
  pInformation->Ctrl_Info.Usb_wOffset = 0;
  (*CopyRoutine) (0);

  return USB_SUCCESS;
}

/*******************************************************************************
* Function Name  : Virtual_Com_Port_NoData_Setup.
* Description    : handle the no data class specific requests.
* Input          : Request Nb.
* Output         : None.
* Return         : USB_UNSUPPORT or USB_SUCCESS.
*******************************************************************************/
static RESULT
Virtual_Com_Port_NoData_Setup (uint8_t RequestNo)
{
  if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    {
      if (RequestNo == SET_COMM_FEATURE)
	return USB_SUCCESS;
      else if (RequestNo == SET_CONTROL_LINE_STATE)
	return USB_SUCCESS;
    }

  return USB_UNSUPPORT;
}

static uint8_t *
gnuk_device_GetDeviceDescriptor (uint16_t Length)
{
  return Standard_GetDescriptorData (Length, &Device_Descriptor);
}

static uint8_t *
gnuk_device_GetConfigDescriptor (uint16_t Length)
{
  return Standard_GetDescriptorData (Length, &Config_Descriptor);
}

static uint8_t *
gnuk_device_GetStringDescriptor (uint16_t Length)
{
  uint8_t wValue0 = pInformation->USBwValue0;

  if (wValue0 > (sizeof (String_Descriptor) / sizeof (ONE_DESCRIPTOR)))
    return NULL;
  else
    return Standard_GetDescriptorData (Length, &String_Descriptor[wValue0]);
}

static RESULT
gnuk_device_Get_Interface_Setting (uint8_t Interface, uint8_t AlternateSetting)
{
  if (AlternateSetting > 0)
    return USB_UNSUPPORT;
  else if (Interface > 1)
    return USB_UNSUPPORT;

  return USB_SUCCESS;
}

/*
 * Interface to USB core
 */

DEVICE_PROP Device_Property = {
  gnuk_device_init,
  gnuk_device_reset,
  gnuk_device_Status_In,
  gnuk_device_Status_Out,
  Virtual_Com_Port_Data_Setup,
  Virtual_Com_Port_NoData_Setup,
  gnuk_device_Get_Interface_Setting,
  gnuk_device_GetDeviceDescriptor,
  gnuk_device_GetConfigDescriptor,
  gnuk_device_GetStringDescriptor,
  0,
  GNUK_MAX_PACKET_SIZE
};

DEVICE Device_Table = {
  EP_NUM,
  1
};

USER_STANDARD_REQUESTS User_Standard_Requests = {
  NOP_Process,			/* GetConfiguration */ 
  gnuk_device_SetConfiguration,
  NOP_Process,			/* GetInterface */
  NOP_Process,			/* SetInterface */
  NOP_Process,			/* GetStatus */
  NOP_Process,			/* ClearFeature */
  NOP_Process,			/* SetEndPointFeature */
  NOP_Process,			/* SetDeviceFeature */
  gnuk_device_SetDeviceAddress
};
