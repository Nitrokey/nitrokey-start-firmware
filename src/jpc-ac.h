/**
 * @brief	Jacobian projective coordinates
 */
typedef struct
{
  bn256 x[1];
  bn256 y[1];
  bn256 z[1];
} jpc;

/**
 * @brief	Affin coordinates
 */
typedef struct
{
  bn256 x[1];
  bn256 y[1];
} ac;

void jpc_double (jpc *X, const jpc *A);
void jpc_add_ac (jpc *X, const jpc *A, const ac *B);
void jpc_add_ac_signed (jpc *X, const jpc *A, const ac *B, int minus);
void jpc_to_ac (ac *X, const jpc *A);
