/*
 * system settings.
 */
#define STM32_PLLXTPRE			STM32_PLLXTPRE_DIV1
#define STM32_PLLMUL_VALUE		6
#define STM32_HSECLK			12000000

#define GPIO_USB_CLEAR_TO_ENABLE       3
#define GPIO_LED_CLEAR_TO_EMIT         4

#define VAL_GPIO_ODR            0xFFFFFFFF
#define VAL_GPIO_CRH            0x88888888      /* PD15...PD8 */
#define VAL_GPIO_CRL            0x88862888      /*  PD7...PD0 */

#define GPIO_USB_BASE	GPIOD_BASE
#define GPIO_LED_BASE	GPIOD_BASE
