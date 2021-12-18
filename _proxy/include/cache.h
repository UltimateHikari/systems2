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
Cache_entry * cache_put(Cache *c, size_t bytes_expected, Request *mdata, char *mime, int mime_len);

Chunk *chunk_init();
int chunk_destroy(Chunk *c);
Chunk * centry_put(Cache_entry *c, char* buf, size_t buflen);
Chunk * centry_pop(Cache_entry *c); // pop uncommitted chunk;
int centry_commit_read(Cache_entry *c, size_t buflen);


#endif