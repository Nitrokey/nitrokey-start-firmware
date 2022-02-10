#include "sys-chip_config.h"
#include <stdint.h>
#include "mcu/sys-stm32f103.h"

//#include "../chopstx/mcu/stm32.h"
#define STM32_USBPRE_DIV1P5     (0 << 22)
#define STM32_USBPRE_DIV2       (3 << 22) /* Not for STM32, but GD32F103 */
#define STM32_ADCPRE_DIV8	(3 << 14)
#define STM32_ADCPRE_DIV6       (2 << 14)

uint8_t detect_hardware (void);


__attribute__((section(".sys.hardwaredef")))
static const struct HardwareDefinition gd32 = {
        .clock = {
                .i_DELIBARATELY_DO_IT_WRONG_START_STOP = 0,
                .i_STM32_USBPRE = STM32_USBPRE_DIV2,
                .i_STM32_PLLMUL_VALUE = 8,
                .i_STM32_ADCPRE = STM32_ADCPRE_DIV8
        }
};

__attribute__((section(".sys.hardwaredef")))
static const struct HardwareDefinition stm32 = {
    .clock = {
            .i_DELIBARATELY_DO_IT_WRONG_START_STOP = 1,
            .i_STM32_USBPRE = STM32_USBPRE_DIV1P5,
            .i_STM32_PLLMUL_VALUE = 6,
            .i_STM32_ADCPRE = STM32_ADCPRE_DIV6
    }
};


static struct HardwareDefinition const * g_current_hardware = NULL;

__attribute__((section(".sys.hardware")))
HardwareDefinitionPtr detect_chip(void) {
//    /*
//    * Check the hardware revision with the following:
//    * 1. set B1 to input-pull up
//    * 2. check if its high - low -> chip is GD32 (rev5), high -> chip is STM32 (rev4)
//    */
//
//    if (g_current_hardware != NULL) {
//        return g_current_hardware;
//    }
//
//    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOB, ENABLE);
//
//    const uint8_t state = gpio_read_pin_input(PB7, 1);
//    if (state == 0)
//    {
//        g_current_hardware = &stm32;
//    } else{
     //   g_current_hardware = &gd32;
//    }

    // Note: at this point we have uninitialized clocks and GPIO

    const uint8_t  hw_rev = detect_hardware();
    if (hw_rev == 5)
        g_current_hardware = &gd32;
    else
        g_current_hardware = &stm32;
    return g_current_hardware;
}
