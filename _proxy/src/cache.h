#ifndef CACHE_H
#define CACHE_H

#include "structs.h"
#include <sys/types.h>

// returns initialized cache 
// or NULL if error happened
Cache * cache_init();
int cache_destroy(Cache * c);
// returns entry on success 
// or NULL if not found
Cache_entry * cache_find(Cache *c, Request* mdata);
Cache_entry * cache_put(Cache *c, size_t bytes_expected, Request *mdata, size_t mci);

#endif