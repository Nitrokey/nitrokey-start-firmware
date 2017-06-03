#include <stdint.h>

uint8_t _regnual_start;
uint8_t __heap_end__;

int
check_crc32 (const uint32_t *start_p, const uint32_t *end_p)
{
  return 0;
}

uint8_t *
sram_address (uint32_t offset)
{
  return ((uint8_t *)0x20000000) + offset;
}

const uint8_t sys_version[8] = {
  3*2+2,	     /* bLength */
  0x03,		     /* bDescriptorType = USB_STRING_DESCRIPTOR_TYPE */
  /* sys version: "3.0" */
  '3', 0, '.', 0, '0', 0,
};

void
led_blink (int spec)
{
}

void
ccid_usb_reset (int full)
{
}

void
ccid_card_change_signal (int how)
{
}

enum ccid_state {
  CCID_STATE_NOCARD,		/* No card available */
  CCID_STATE_START,		/* Initial */
  CCID_STATE_WAIT,		/* Waiting APDU */
				/* Busy1, Busy2, Busy3, Busy5 */
  CCID_STATE_EXECUTE,		/* Busy4 */
  CCID_STATE_RECEIVE,		/* APDU Received Partially */
  CCID_STATE_SEND,		/* APDU Sent Partially */

  CCID_STATE_EXITED,		/* ICC Thread Terminated */
  CCID_STATE_EXEC_REQUESTED,	/* Exec requested */
};

static enum ccid_state ccid_state;
enum ccid_state *const ccid_state_p = &ccid_state;
