void random_init (void);

/* 32-byte random bytes */
const uint8_t *random_bytes_get (void);
void random_bytes_free (const uint8_t *p);

/* 4-byte salt */
uint32_t get_salt (void);

/* iterator returning a byta at a time */
uint8_t random_byte (void *arg);
