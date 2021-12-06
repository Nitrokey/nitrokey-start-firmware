#include "stddef.h"

#ifndef NITROKEY_START_FIRMWARE_CHIP_CONFIG
#define NITROKEY_START_FIRMWARE_CHIP_CONFIG

struct HardwareDefinition{
    struct {
        int i_DELIBARATELY_DO_IT_WRONG_START_STOP;
        int i_STM32_USBPRE;
        int i_STM32_PLLMUL_VALUE;
        int i_STM32_ADCPRE;
    } clock;
};

typedef struct HardwareDefinition const * const HardwareDefinitionPtr;
HardwareDefinitionPtr detect_chip(void);

#endif
