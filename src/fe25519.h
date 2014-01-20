#define FE25519_WORDS 8
typedef struct fe25519 {
  uint32_t word[FE25519_WORDS]; /* Little endian */
} fe25519;
