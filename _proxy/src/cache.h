#include <pthread.h>
#include <sys/types.h>
#include "verify.h"
#include <stdlib.h>
#define E_DESTROY -1
#define BE_INF -1
#define MCI_DEAD -1
#define MCI_DONE 0

/**
 * CacheEntries forming linked list
 * instead of hash table;
 * if highload performance is a concern
 * it can easily be swapped.
 */

typedef struct Chunk{
	size_t size;
	char * data;

	struct Chunk *next;
} Chunk;

/**
 * not bringing dns resolution logic in here
 * so GET google.com and GET whatever it ip is
 * will be cached as 2 separate responces;
 */

typedef struct {
	size_t type;
	char * hostname;
	size_t port;
	char * mime;
} Request_metadata;

typedef struct Cache_entry{
	size_t bytes_ready;
	//can = BE_INF if no body-length was provided
	size_t bytes_expected;
	Request_metadata * mdata;

	/**
	 * id of connection that is caching response now
	 * if = MCI_DEAD - first stumbled reader must become master
	 * if = MCI_DONE - no need in lagging mechanisms, 
	 * everyone can read freely
	 */
	size_t master_connection_id;

	// for bootstrapping readers as master caches
	pthread_mutex_t lag_lock;
	pthread_cond_t lag_cond;

	// data for collector (changing with cache's collector lock)
	size_t readers_amount;
	size_t last_access_ms;

	Chunk *head;
	struct Cache_entry *next;
} Cache_entry;

typedef struct {
	size_t chunk_size_bytes;
	size_t max_size_bytes;
	size_t collect_threshold_percent;

	// lock for preserving correctness of iteration
	// when list pointers getting rearrange
	pthread_mutex_t structural_lock;

	// lock for snapshotting cache state
	// when collector collects
	pthread_mutex_t collector_lock;

	Cache_entry *head;
} Cache;

// returns initialized cache 
// or NULL if error happened
Cache * cache_init();

int cache_destroy(Cache * c);

// returns entry on success 
// or NULL if not found
Cache_entry * cache_find(Cache *c, Request_metadata* mdata);

Cache_entry * cache_put(Cache *c, Request_metadata *mdata, size_t mci);
