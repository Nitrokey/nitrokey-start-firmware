#ifdef GNU_LINUX_EMULATION
#define SIZE_1 4096
#define SIZE_2 4096
#define SIZE_3 (5 * 4096)
#else
#define SIZE_0 0x0160 /* Main         */
#define SIZE_1 0x01a0 /* CCID         */
#define SIZE_2 0x0180 /* RNG          */
#if MEMORY_SIZE >= 32
#define SIZE_3 0x4640 /* openpgp-card */
#elif MEMORY_SIZE >= 24
#define SIZE_3 0x2640 /* openpgp-card */
#else
#define SIZE_3 0x1640 /* openpgp-card */
#endif
#define SIZE_4 0x0000 /* ---          */
#define SIZE_5 0x0200 /* msc          */
#define SIZE_6 0x00c0 /* timer (cir)  */
#define SIZE_7 0x00c0 /* ext   (cir)  */
#endif

#if defined(STACK_MAIN) && !defined(GNU_LINUX_EMULATION) 
/* Idle+Exception handlers */
char __main_stack_end__[0] __attribute__ ((section(".main_stack")));
char main_base[0x0080] __attribute__ ((section(".main_stack")));

/* Main program            */
char __process0_stack_end__[0] __attribute__ ((section(".process_stack.0")));
char process0_base[SIZE_0] __attribute__ ((section(".process_stack.0")));
#endif

/* First thread program    */
#if defined(STACK_PROCESS_1)
char process1_base[SIZE_1] __attribute__ ((section(".process_stack.1"))); 
#endif

/* Second thread program   */
#if defined(STACK_PROCESS_2)
char process2_base[SIZE_2] __attribute__ ((section(".process_stack.2")));
#endif

/* Third thread program    */
#if defined(STACK_PROCESS_3)
char process3_base[SIZE_3] __attribute__ ((section(".process_stack.3")));
#endif

/* Fourth thread program    */
#if defined(STACK_PROCESS_4)
char process4_base[SIZE_4] __attribute__ ((section(".process_stack.4")));
#endif

/* Fifth thread program    */
#if defined(STACK_PROCESS_5)
char process5_base[SIZE_5] __attribute__ ((section(".process_stack.5")));
#endif

/* Sixth thread program    */
#if defined(STACK_PROCESS_6)
char process6_base[SIZE_6] __attribute__ ((section(".process_stack.6")));
#endif

/* Seventh thread program    */
#if defined(STACK_PROCESS_7)
char process7_base[SIZE_7] __attribute__ ((section(".process_stack.7")));
#endif
