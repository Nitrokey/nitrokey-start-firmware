int compute_kP (ac *X, const bn256 *K, const ac *P);

int compute_kG (ac *X, const bn256 *K);
void ecdsa (bn256 *r, bn256 *s, const bn256 *z, const bn256 *d);

