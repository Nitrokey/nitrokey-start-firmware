extern const bn256 p256;
#define P256 (&p256)

void modp256_add (bn256 *X, const bn256 *A, const bn256 *B);
void modp256_sub (bn256 *X, const bn256 *A, const bn256 *B);
void modp256_reduce (bn256 *X, const bn512 *A);
void modp256_mul (bn256 *X, const bn256 *A, const bn256 *B);
void modp256_sqr (bn256 *X, const bn256 *A);
void modp256_inv (bn256 *C, const bn256 *a);
void modp256_shift (bn256 *X, const bn256 *A, int shift);
