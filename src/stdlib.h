/*
 * stdlib.h replacement to replace malloc functions
 */

typedef unsigned int size_t;

#include <stddef.h> /* NULL */

#define malloc(size)	gnuk_malloc (size)
#define free(p)		gnuk_free (p)

void *gnuk_malloc (size_t);
void gnuk_free (void *);
