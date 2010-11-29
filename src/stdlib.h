/*
 * stdlib.h replacement, so that we can replace malloc functions
 */

typedef unsigned int size_t;

#include "ch.h"
#include "chheap.h"
#define malloc(size)	chHeapAlloc (NULL, size)
#define free(p)		chHeapFree (p)
