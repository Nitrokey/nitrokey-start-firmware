#include "stddef.h"
#include <stdint.h>

#ifndef NITROKEY_START_FIRMWARE_CHIP_CONFIG
#define NITROKEY_START_FIRMWARE_CHIP_CONFIG

//extern uint8_t hw_rev;

struct HardwareDefinition{
    struct {
        uint8_t i_DELIBARATELY_DO_IT_WRONG_START_STOP;
        int i_STM32_USBPRE;
        uint8_t i_STM32_PLLMUL_VALUE;
        int i_STM32_ADCPRE;
    } clock;
};

typedef struct HardwareDefinition const * const HardwareDefinitionPtr;
HardwareDefinitionPtr detect_chip(void);

#endif
