/*
 * Common code for hwinit0
 */

#ifdef DFU_SUPPORT
  SCB->VTOR = 0x08003000;
#endif

  stm32_clock_init();
