/* Hardware specific function */

#include "ch.h"
#include "hal.h"
#include "board.h"

const uint8_t *
unique_device_id (void)
{
  /* STM32F103 has 96-bit unique device identifier */
  const uint8_t *addr = (const uint8_t *)0x1ffff7e8;

  return addr;
}
