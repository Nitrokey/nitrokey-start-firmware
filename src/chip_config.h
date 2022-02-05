#include "stddef.h"
#include <stdint.h>

#ifndef NITROKEY_START_FIRMWARE_CHIP_CONFIG
#define NITROKEY_START_FIRMWARE_CHIP_CONFIG

//extern uint8_t hw_rev;

struct HardwareDefinition{
    struct {
        int i_STM32_USBPRE;
        int i_STM32_ADCPRE;
        uint8_t i_DELIBARATELY_DO_IT_WRONG_START_STOP;
        uint8_t i_STM32_PLLMUL_VALUE;
    } clock;
};

typedef struct HardwareDefinition const * const HardwareDefinitionPtr;
HardwareDefinitionPtr detect_chip(void);

#endif
