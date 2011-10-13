typedef struct naf4_257 {
  uint32_t words[ BN256_WORDS*4 ]; /* Little endian */
  uint8_t last_nibble;		   /* most significant nibble */
} naf4_257;

void compute_naf4_257 (naf4_257 *NAF_K, const bn256 *K);
void compute_kP (ac *X, const naf4_257 *NAF_K, const ac *P);

void compute_kG (ac *X, const bn256 *K);
void ecdsa (bn256 *r, bn256 *s, const bn256 *z, const bn256 *d);

