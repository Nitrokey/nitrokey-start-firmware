/*
 * stdlib.h replacement, so that we can replace malloc functions
 */

typedef unsigned int size_t;

#ifdef REPLACE_MALLOC
#define malloc my_malloc
#define free my_free
#define realloc my_realloc
#endif

extern void *malloc (size_t size);
extern void free (void *ptr);
extern void *realloc (void *ptr, size_t size);
