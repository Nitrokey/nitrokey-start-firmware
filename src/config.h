#ifdef DEBUG
#define ENABLE_VIRTUAL_COM_PORT 1
#endif

/* Packet size of USB Bulk transfer for full speed */
#define GNUK_MAX_PACKET_SIZE 64

#if 0
/* FSIJ */
#define MANUFACTURER_IN_AID		0xf5, 0x17
#else
/* for random serial number*/
#define MANUFACTURER_IN_AID		0xff, 0xfe
#endif

#define SERIAL_NUMBER_IN_AID  0x00, 0x00, 0x00, 0x01
