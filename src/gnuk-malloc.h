/*
 * Gnuk uses its own malloc functions.
 *
 * The intention is no-dependency to C library.  But, we provide
 * malloc and free here, since RSA routines uses malloc/free
 * internally.
 *
 */

#include <stddef.h> /* NULL and size_t */

#define malloc(size)	gnuk_malloc (size)
#define free(p)		gnuk_free (p)

void *gnuk_malloc (size_t);
void gnuk_free (void *);
