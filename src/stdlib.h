/*
 * stdlib.h replacement to replace malloc functions
 *
 * The intention is that no dependency to C library.  But since RSA
 * routines uses malloc/free, we provide malloc and free.
 *
 * For GNU/Linux emulation, we can use C library.
 */

#include <stddef.h> /* NULL and size_t */

#define malloc(size)	gnuk_malloc (size)
#define free(p)		gnuk_free (p)

void *gnuk_malloc (size_t);
void gnuk_free (void *);

#ifdef GNU_LINUX_EMULATION
long int random(void);
void srandom(unsigned int seed);
void exit(int status);
#endif
