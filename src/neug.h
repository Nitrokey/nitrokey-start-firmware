#define NEUG_NO_KICK      0
#define NEUG_KICK_FILLING 1

#define NEUG_PRE_LOOP 16

void neug_init (uint32_t *buf, uint8_t size);
void neug_prng_reseed (void);
uint32_t neug_get (int kick);
void neug_kick_filling (void);
void neug_flush (void);
void neug_wait_full (void);
