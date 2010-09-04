/*
 *
 */

#include "config.h"
#include "usb_lib.h"
#include "usb_desc.h"

#define USB_ICC_INTERFACE_CLASS 0x0B
#define USB_ICC_INTERFACE_SUBCLASS 0x00
#define USB_ICC_INTERFACE_BULK_PROTOCOL 0x00
#define USB_ICC_DATA_SIZE 64

/* USB Standard Device Descriptor */
static const uint8_t gnukDeviceDescriptor[] = {
  18,   /* bLength */
  USB_DEVICE_DESCRIPTOR_TYPE,     /* bDescriptorType */
  0x00, 0x02,   /* bcdUSB = 2.00 */
  0x00,   /* bDeviceClass: 0 means deferred to interface */
  0x00,   /* bDeviceSubClass */
  0x00,   /* bDeviceProtocol */
  0x40,   /* bMaxPacketSize0 */
  0xff, 0xff,   /* idVendor = 0xffff */
  0x01, 0x00,   /* idProduct = 0x0001 */
  0x00, 0x02,   /* bcdDevice = 2.00 */
  1, /* Index of string descriptor describing manufacturer */
  2, /* Index of string descriptor describing product */
  3, /* Index of string descriptor describing the device's serial number */
  0x01    /* bNumConfigurations */
};

#ifdef ENABLE_VIRTUAL_COM_PORT
#define W_TOTAL_LENGTH (9+9+54+7+7+9+5+5+4+5+7+9+7+7)
#define NUM_INTERFACES 3	/* two for CDC, one for GPG */
#else
#define W_TOTAL_LENGTH (9+9+54+7+7)
#define NUM_INTERFACES 1	/* GPG only */
#endif

/* Configuation Descriptor */
static const uint8_t gnukConfigDescriptor[] = {
  9,			   /* bLength: Configuation Descriptor size */
  USB_CONFIGURATION_DESCRIPTOR_TYPE,      /* bDescriptorType: Configuration */
  W_TOTAL_LENGTH, 0x00,   /* wTotalLength:no of returned bytes */
  NUM_INTERFACES,   /* bNumInterfaces: */
  0x01,   /* bConfigurationValue: Configuration value */
  0x00,   /* iConfiguration: Index of string descriptor describing the configuration */
  0xC0,   /* bmAttributes: self powered */
  50,	  /* MaxPower 100 mA */

  /* Interface Descriptor */
  9,			      /* bLength: Interface Descriptor size */
  USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType: Interface */
  0,				 /* Index of this interface */
  0,			    /* Alternate setting for this interface */
  2,			    /* bNumEndpoints: Bulk-IN, Bulk-OUT */
  USB_ICC_INTERFACE_CLASS,
  USB_ICC_INTERFACE_SUBCLASS,
  USB_ICC_INTERFACE_BULK_PROTOCOL,
  0,				/* string index for interface */

  /* ICC Descriptor */
  54,			  /* bLength: */
  0x21,			  /* bDescriptorType: USBDESCR_ICC */
  0x10, 0x01,		  /* bcdCCID: 1.1 XXX */
  0,			  /* bMaxSlotIndex: */
  1,			  /* bVoltageSupport: FIXED VALUE */
  0x02, 0, 0, 0,	  /* dwProtocols: T=1 */
  0xfc, 0x0d, 0, 0,	  /* dwDefaultClock: FIXED VALUE */
  0xfc, 0x0d, 0, 0,	  /* dwMaximumClock: FIXED VALUE*/
  1,			  /* bNumClockSupported: FIXED VALUE*/
  0x80, 0x25, 0, 0,	  /* dwDataRate: FIXED VALUE */
  0x80, 0x25, 0, 0,	  /* dwMaxDataRate: FIXED VALUE */
  1,			  /* bNumDataRateSupported: FIXED VALUE */
  0xfe, 0, 0, 0,	  /* dwMaxIFSD:  */
  0, 0, 0, 0,		  /* dwSynchProtocols: FIXED VALUE */
  0, 0, 0, 0,		  /* dwMechanical: FIXED VALUE */
  0x40, 0x08, 0x04, 0x00, /* dwFeatures: Short and extended APDU level */
  0x40, 0x00, 0, 0,	  /* dwMaxCCIDMessageLength: 64 */
  0xff,			  /* bClassGetResponse: */
  0xff,			  /* bClassEnvelope: */
  0, 0,			  /* wLCDLayout: FIXED VALUE */
  0,			  /* bPinSupport: No PIN pad */
  1,			  /* bMaxCCIDBusySlots: 1 */
  /*Endpoint 1 Descriptor*/
  7,			       /* bLength: Endpoint Descriptor size */
  USB_ENDPOINT_DESCRIPTOR_TYPE,	/* bDescriptorType: Endpoint */
  0x81,				/* bEndpointAddress: (IN1) */
  0x02,				/* bmAttributes: Bulk */
  USB_ICC_DATA_SIZE, 0x00,      /* wMaxPacketSize: */
  0x00,				/* bInterval */
  /*Endpoint 2 Descriptor*/
  7,			       /* bLength: Endpoint Descriptor size */
  USB_ENDPOINT_DESCRIPTOR_TYPE,	/* bDescriptorType: Endpoint */
  0x02,				/* bEndpointAddress: (OUT2) */
  0x02,				/* bmAttributes: Bulk */
  USB_ICC_DATA_SIZE, 0x00,	/* wMaxPacketSize: */
  0x00,				/* bInterval */
#ifdef ENABLE_VIRTUAL_COM_PORT
  /* Interface Descriptor */
  9,			      /* bLength: Interface Descriptor size */
  USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType: Interface */
  0x01,		  /* bInterfaceNumber: Number of Interface */
  0x00,		  /* bAlternateSetting: Alternate setting */
  0x01,		  /* bNumEndpoints: One endpoints used */
  0x02,		  /* bInterfaceClass: Communication Interface Class */
  0x02,		  /* bInterfaceSubClass: Abstract Control Model */
  0x01,		  /* bInterfaceProtocol: Common AT commands */
  0x00,		  /* iInterface: */
  /*Header Functional Descriptor*/
  5,			    /* bLength: Endpoint Descriptor size */
  0x24,			    /* bDescriptorType: CS_INTERFACE */
  0x00,			    /* bDescriptorSubtype: Header Func Desc */
  0x10,			    /* bcdCDC: spec release number */
  0x01,
  /*Call Managment Functional Descriptor*/
  5,	    /* bFunctionLength */
  0x24,	    /* bDescriptorType: CS_INTERFACE */
  0x01,	    /* bDescriptorSubtype: Call Management Func Desc */
  0x03,	    /* bmCapabilities: D0+D1 */
  0x02,	    /* bDataInterface: 2 */
  /*ACM Functional Descriptor*/
  4,	    /* bFunctionLength */
  0x24,	    /* bDescriptorType: CS_INTERFACE */
  0x02,	    /* bDescriptorSubtype: Abstract Control Management desc */
  0x02,	    /* bmCapabilities */
  /*Union Functional Descriptor*/
  5,		 /* bFunctionLength */
  0x24,		 /* bDescriptorType: CS_INTERFACE */
  0x06,		 /* bDescriptorSubtype: Union func desc */
  0x01,		 /* bMasterInterface: Communication class interface */
  0x02,		 /* bSlaveInterface0: Data Class Interface */
  /*Endpoint 4 Descriptor*/
  7,			       /* bLength: Endpoint Descriptor size */
  USB_ENDPOINT_DESCRIPTOR_TYPE,	   /* bDescriptorType: Endpoint */
  0x84,				   /* bEndpointAddress: (IN4) */
  0x03,				   /* bmAttributes: Interrupt */
  VIRTUAL_COM_PORT_INT_SIZE, 0x00, /* wMaxPacketSize: */
  0xFF,				   /* bInterval: */

  /*Data class interface descriptor*/
  9,			       /* bLength: Endpoint Descriptor size */
  USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType: */
  0x02,			   /* bInterfaceNumber: Number of Interface */
  0x00,			   /* bAlternateSetting: Alternate setting */
  0x02,			   /* bNumEndpoints: Two endpoints used */
  0x0A,			   /* bInterfaceClass: CDC */
  0x00,			   /* bInterfaceSubClass: */
  0x00,			   /* bInterfaceProtocol: */
  0x00,			   /* iInterface: */
  /*Endpoint 5 Descriptor*/
  7,			       /* bLength: Endpoint Descriptor size */
  USB_ENDPOINT_DESCRIPTOR_TYPE,	    /* bDescriptorType: Endpoint */
  0x05,				    /* bEndpointAddress: (OUT5) */
  0x02,				    /* bmAttributes: Bulk */
  VIRTUAL_COM_PORT_DATA_SIZE, 0x00, /* wMaxPacketSize: */
  0x00,			     /* bInterval: ignore for Bulk transfer */
  /*Endpoint 3 Descriptor*/
  7,			       /* bLength: Endpoint Descriptor size */
  USB_ENDPOINT_DESCRIPTOR_TYPE,	    /* bDescriptorType: Endpoint */
  0x83,				    /* bEndpointAddress: (IN3) */
  0x02,				    /* bmAttributes: Bulk */
  VIRTUAL_COM_PORT_DATA_SIZE, 0x00, /* wMaxPacketSize: */
  0x00				    /* bInterval */
#endif
};


/* USB String Descriptors */
static const uint8_t gnukStringLangID[] = {
  4,				/* bLength */
  USB_STRING_DESCRIPTOR_TYPE,
  0x09, 0x04			/* LangID = 0x0409: US-English */
};

static const uint8_t gnukStringVendor[] = {
  33*2+2,			/* bLength */
  USB_STRING_DESCRIPTOR_TYPE,	/* bDescriptorType*/
  /* Manufacturer: "Free Software Initiative of Japan" */
  'F', 0, 'r', 0, 'e', 0, 'e', 0, ' ', 0, 'S', 0, 'o', 0, 'f', 0,
  't', 0, 'w', 0, 'a', 0, 'r', 0, 'e', 0, ' ', 0, 'I', 0, 'n', 0,
  'i', 0, 't', 0, 'i', 0, 'a', 0, 't', 0, 'i', 0, 'v', 0, 'e', 0,
  ' ', 0, 'o', 0, 'f', 0, ' ', 0, 'J', 0, 'a', 0, 'p', 0, 'a', 0,
  'n', 0
};

static const uint8_t gnukStringProduct[] = {
  14*2+2,			/* bLength */
  USB_STRING_DESCRIPTOR_TYPE,	/* bDescriptorType */
  /* Product name: "FSIJ USB Token" */
  'F', 0, 'S', 0, 'I', 0, 'J', 0, ' ', 0, 'U', 0, 'S', 0, 'B', 0,
  ' ', 0, 'T', 0, 'o', 0, 'k', 0, 'e', 0, 'n', 0
};

static const uint8_t gnukStringSerial[] = {
  8,				/* bLength */
  USB_STRING_DESCRIPTOR_TYPE,	/* bDescriptorType */
  '2', 0, '.', 0, '0', 0
};

const ONE_DESCRIPTOR Device_Descriptor = {
  (uint8_t*)gnukDeviceDescriptor,
  sizeof (gnukDeviceDescriptor)
};

const ONE_DESCRIPTOR Config_Descriptor = {
  (uint8_t*)gnukConfigDescriptor,
  sizeof (gnukConfigDescriptor)
};

const ONE_DESCRIPTOR String_Descriptor[4] = {
  {(uint8_t*)gnukStringLangID, sizeof (gnukStringLangID)},
  {(uint8_t*)gnukStringVendor, sizeof (gnukStringVendor)},
  {(uint8_t*)gnukStringProduct, sizeof (gnukStringProduct)},
  {(uint8_t*)gnukStringSerial, sizeof (gnukStringSerial)},
};
