/* HAL configuration file for ChibiOS/RT */

#ifndef _HALCONF_H_
#define _HALCONF_H_

#include "mcuconf.h"

#define HAL_USE_PAL              TRUE
#define HAL_USE_ADC              FALSE
#define HAL_USE_CAN              FALSE
#define HAL_USE_EXT              FALSE
#define HAL_USE_GPT              FALSE
#define HAL_USE_I2C              FALSE
#define HAL_USE_ICU              FALSE
#define HAL_USE_MAC              FALSE
#define HAL_USE_MMC_SPI          FALSE
#define HAL_USE_PWM              FALSE
#define HAL_USE_RTC              FALSE
#define HAL_USE_SDC              FALSE
#define HAL_USE_SERIAL           FALSE
#define HAL_USE_SERIAL_USB       FALSE
#define HAL_USE_SPI              FALSE
#define HAL_USE_UART             FALSE
#define HAL_USE_USB              FALSE

/* Define manually, as we implement ADC driver by ourselves.  */
#define STM32_DMA_REQUIRED	1

#endif /* _HALCONF_H_ */
