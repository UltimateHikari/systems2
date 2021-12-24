#ifndef STRUCTS_H
#define STRUCTS_H
#include <semaphore.h>

#define PARSE 0
#define READ 1

#define BE_INF 10*1024*1024
#define MCI_ALIVE 1
#define MCI_DEAD -1
#define MCI_DONE 0

//TODO move this block into config too;
#define REQBUFSIZE 4096
#define MAX_THREADS 100
#define WORKERS_AMOUNT 2 

/**
 * CacheEntries forming linked list
 * instead of hash table;
 * if highload performance is a concern
 * it can easily be swapped.
 */

typedef struct Chunk{
	size_t size;
	char data[REQBUFSIZE];

	struct Chunk *next;
} Chunk;

typedef struct {
	char buf[REQBUFSIZE];
	char *hostname;
	size_t buflen;
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
	int mime_len;

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
	Chunk *last;
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

typedef struct D_entry{
	void *conn;
	struct D_entry *next;
}D_entry;

struct Worker;
struct Dispatcher;

enum Stale{
	IsStale,
	Updated
};

typedef struct{
	int isListenerAlive;
	Cache *cache;
	int num_threads;
	pthread_t threads[MAX_THREADS];
	pthread_mutex_t dpatch_lock;
	pthread_cond_t dpatch_cond;
	struct Worker * head_worker;
	D_entry * head_conn;
	D_entry * last_conn;
	int labclass;
	int isStale;
} Dispatcher;

enum WState{
	Working,
	Empty
};

typedef struct Worker{
	int state; 
	sem_t latch; // enables to handle body one more time
	void *arg; // client or server connection;
	void* (*body)(void*);
	pthread_t thread;
	struct Worker *next;
	Dispatcher *d;
}Worker;

enum State{
	Parse,
	Read, // from cache for client and to cache for server
	Proxy,
	Done
};

#define SERVER 'S'
#define CLIENT 'C'

typedef struct{
	char type; // for type deducing, must be SERVER
	int socket;
	Cache_entry *entry;
	int state;
	char buf[REQBUFSIZE];
	size_t buflen;
	Dispatcher *d;
	size_t debug_chunk_number;
} Server_Connection;

typedef struct{
	char type; // for type deducing, must be CLIENT
	int socket;
	Cache *cache;
	Cache_entry *entry;
	Request *request;
	int state;
	size_t bytes_read;
	Server_Connection *c;
	Dispatcher *d;
	Chunk *current;
	int is_registered;
	size_t debug_chunk_number;
} Client_connection;

typedef struct{
	int socket;
} Listener;

#endif