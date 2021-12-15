#ifndef STRUCTS_H
#define STRUCTS_H

#define PARSE 0
#define READ 1
#define REQBUFSIZE 1024

#define BE_INF 10*1024*1024
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

typedef struct {
	char buf[REQBUFSIZE];
	char *hostname;
	size_t hostname_len;
} Request;

/**
 * not bringing dns resolution logic in here
 * so GET google.com and GET whatever it ip is
 * will be cached as 2 separate responces;
 */

typedef struct Cache_entry{
	size_t bytes_ready;
	//can = BE_INF if no body-length was provided
	size_t bytes_expected;
	Request * mdata;
	char * mime;

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

	Chunk *head;
	struct Cache_entry *next;
} Cache_entry;

typedef struct {
	size_t chunk_size_bytes;
	size_t max_size_bytes;
	size_t current_expected_bytes;
	size_t collect_threshold_percent;

	// lock for preserving correctness of iteration
	// when list entries can get removed/added
	pthread_mutex_t structural_lock;

	// head is oldest, kept by adding as last
	Cache_entry *head;
	Cache_entry *last;
	Cache_entry *marked;
} Cache;

enum State{
	Parse,
	Read, // from cache
	Proxy,
	Done
};

typedef struct{
	int socket;
	Cache *cache;
	Cache_entry *entry;
	Request *request;
	int state;
	int labclass;
	size_t bytes_read;
} Client_connection;

typedef struct{
	int socket;
	Cache_entry *entry;
} Server_Connection;

#endif