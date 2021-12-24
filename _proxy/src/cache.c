#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

#include "client.h"
#include "verify.h"
#include "prerror.h"
#include "cache.h"
#include "logger.h"
#include "server.h"

// Translation unit-local funcs
void *cache_cleanup(void *raw_cache);
int mdata_is_equal(Request * a, Request *b);
size_t curtime();

Cache_entry * centry_init(size_t bytes_expected, Request *mdata, char *mime, int mime_len);
int centry_destroy(Cache_entry * c);

Cache *cache_free_marked(Cache *c);
int cache_garbage_check(Cache *c, size_t bytes_expected);
Cache_entry * cache_garbage_collect(Cache *c, size_t bytes_to_collect);
bool is_eligible_to_collect(Cache_entry *c);

// cleanup

void *cache_cleanup(void *raw_cache){
	LOG_INFO("cache_cleanup");
	panic_signal(); // cache failure is panic material
	RETURN_NULL_IF_NULL(raw_cache);
	Cache ** cache = (Cache **)raw_cache;
	RETURN_NULL_IF_NULL(*cache);

	cache_destroy(*cache);
	*cache = NULL;
	pthread_exit(NULL);
}

void *centry_cleanup(void *raw_centry){
	LOG_INFO("centry_cleanup");
	RETURN_NULL_IF_NULL(raw_centry);
	Cache_entry** centry = (Cache_entry**)raw_centry;
	RETURN_NULL_IF_NULL(*centry);
	centry_destroy(*centry);
	*centry = NULL;
	return NULL;
}

// Local  implementations


int mdata_is_equal(Request * a, Request *b){
	LOG_DEBUG("mdata_is_equal-call");
	if(a == NULL || b == NULL || a->hostname == NULL || b->hostname == NULL || a->hostname_len == (size_t)E_COMPARE){
		return E_COMPARE;
	}
	return (strncmp(a->hostname, b->hostname, a->hostname_len) == 0);
}

Cache_entry * centry_init(size_t bytes_expected, Request *mdata, char *mime, int mime_len){
	Cache_entry * res = (Cache_entry*)malloc(sizeof(Cache_entry));
	RETURN_NULL_IF_NULL(res);

	res->bytes_ready = 0;
	res->bytes_expected = bytes_expected;
	res->mdata = mdata;
	res->mime = mime;
	res->mime_len = mime_len;
	res->master_connection_id = MCI_ALIVE;
	res->readers_amount = 1;
	res->head = NULL;
	res->head = NULL;
	res->next = NULL;

	void* arg = ((void*)&res);
	verify(pthread_mutex_init(&(res->lag_lock), NULL), "l_lock init", centry_cleanup, arg);
	verify(pthread_cond_init(&(res->lag_cond), NULL),  "l_cond init", centry_cleanup, arg);
	return res;
}

int centry_destroy(Cache_entry * c){
	LOG_DEBUG("centry_destroy-call");
	if(c == NULL){
		return E_DESTROY;
	}

	if(c->mime != NULL){
		free(c->mime);
	}

	Chunk *current = c->head;
	Chunk *next;
	while(current != NULL){
		next = current->next;
		chunk_destroy(current);
		current = next;
	}
	pthread_mutex_destroy(&(c->lag_lock));
	pthread_cond_destroy(&(c->lag_cond));
	free(c);

	return S_DESTROY;
}

// can just append empty chunk
Chunk * centry_put(Cache_entry *c, char* buf, size_t buflen){
	Chunk *chunk = (Chunk *)malloc(sizeof(Chunk));
	RETURN_NULL_IF_NULL(chunk);
	chunk->size = 0;
	chunk->next = NULL;

	if(c->head == NULL){
		c->head = chunk;
		c->last = chunk;
	}else{
		c->last->next = chunk;
		c->last = chunk;
	}
	// TODO: if c->bytes_ready + buflen > bytes_excepted -> error or going to proxy mode;
	if(buf != NULL){
		// for puttring request;
		strncpy(chunk->data, buf, buflen);
		chunk->size = buflen;
		lag_broadcast(c, buflen);
	}
	return chunk;
}

Chunk * centry_pop(Cache_entry *c){
 	/* pop uncommitted chunk
 	expected to be used rarely, so sub-optimal*/;
 	RETURN_NULL_IF_NULL(c);
	return NULL;
}

int chunk_destroy(Chunk *c){
	if(c == NULL){
		return E_DESTROY;
	}
	free(c);
	return S_DESTROY;
}

int cache_garbage_check(Cache *c, size_t bytes_expected){
	LOG_DEBUG("cache_garbage_check-call");
	int res = S_CHECK;

	size_t threshold = c->max_size_bytes * c->collect_threshold_percent / 100;
	size_t expected = c->current_expected_bytes + bytes_expected;
	if(threshold < expected){
		c->marked = cache_garbage_collect(c, expected - threshold);
		if(threshold < c->current_expected_bytes + bytes_expected){
			// threshold not met after gc, no caching
			res = E_NOSPACE;
		}
	}

	return res;
}

Cache_entry * cache_garbage_collect(Cache *c, size_t bytes_to_collect){
	LOG_DEBUG("cache_garbage_collect-call");
	Cache_entry *current = c->head;
	Cache_entry *previous = NULL;
	Cache_entry *marked_head = NULL;
	Cache_entry *marked_last = NULL;

	size_t bytes_collected = 0;
	void*arg = (void*)&c;
	if(verify(pthread_mutex_lock(&(c->structural_lock)), "collect start", cache_cleanup, arg) < 0){return NULL;};
		while(current != NULL && bytes_collected < bytes_to_collect){
			Cache_entry *next = current->next;
			if(is_eligible_to_collect(current)){
					c->current_expected_bytes = c->current_expected_bytes - current->bytes_expected;

					if(previous != NULL){
						previous->next = next;
					}
					if(marked_head == NULL){
						marked_head = current;
						marked_last = current;
					}else{
						marked_last->next = current;
					}
					current->next = NULL;

			} else {
				previous = current;
			}
			current = next;
		}
	if(verify(pthread_mutex_unlock(&(c->structural_lock)), "collect end", cache_cleanup, arg) < 0){return NULL;};
	return marked_head;
}

bool is_eligible_to_collect(Cache_entry *c){
	// TODO check for active server connection, kill or sth
	return (c->readers_amount == 0);
}

// Header implementations

Cache * cache_init(){
	Cache * res = (Cache*)malloc(sizeof(Cache));
	RETURN_NULL_IF_NULL(res);

	res->chunk_size_bytes = CHUNK_SIZE_BYTES;
	res->max_size_bytes = MAX_SIZE_BYTES;
	res->collect_threshold_percent = DEFAULT_THRESHOLD;
	res->current_expected_bytes = 0;
	res->head = NULL;
	res->last = NULL;
	res->marked = NULL;

	void* arg = (void*)&res;
	verify(pthread_mutex_init(&(res->structural_lock), NULL), "s_lock init", cache_cleanup, arg);
	return res;
}

int cache_destroy(Cache * c){
	if(c == NULL){
		return E_DESTROY;
	}

	Cache_entry *current = c->head;
	Cache_entry *next;
	while(current != NULL){
		next = current->next;
		centry_destroy(current);
		current = next;
	}
	pthread_mutex_destroy(&(c->structural_lock));
	free(c);

	return S_DESTROY;
}

// returns entry on success 
// or NULL if not found
Cache_entry * cache_find(Cache * c, Request* mdata){
	LOG_DEBUG("cache_find-call");
	RETURN_NULL_IF_NULL(c);
	Cache_entry *current = c->head;

	void*arg = (void*)&c;
	while(current !=  NULL){
		if(mdata_is_equal(mdata, current->mdata)){
			return current;
		}
		if(verify(pthread_mutex_lock(&(c->structural_lock)), "find starting iter", cache_cleanup, arg) < 0){return NULL;};
			current = current->next;
		if(verify(pthread_mutex_unlock(&(c->structural_lock)), "not found ending iter", cache_cleanup, arg) < 0){return NULL;};

	}
	return NULL;
}

Cache_entry * cache_put(Cache *c, size_t bytes_expected, Request *mdata, char *mime, int mime_len){
	LOG_DEBUG("cache_put-call");
	Cache_entry * newentry = centry_init(bytes_expected, mdata, mime, mime_len);

	void*arg = (void*)&c;
	if(cache_garbage_check(c, bytes_expected) == E_NOSPACE){
		centry_destroy(newentry);
		cache_free_marked(c);
		return NULL;
	}
	if(verify(pthread_mutex_lock(&(c->structural_lock)), "adding entry", cache_cleanup, arg) < 0){return NULL;};
		if(c->head == NULL){
			c->head = newentry;
			c->last = newentry;
		}else{
			c->last->next = newentry;
			c->last = newentry;
		}
		c->current_expected_bytes += bytes_expected;
	if(verify(pthread_mutex_unlock(&(c->structural_lock)), "adding entry", cache_cleanup, arg) < 0){return NULL;};

	cache_free_marked(c);
	return newentry;
}

Cache *cache_free_marked(Cache *c){
	Cache_entry *marked = c->marked;
	while(c->marked != NULL){
		centry_destroy(marked);
		marked = marked->next;
	}
	c->marked = NULL;
	return c;
}

int centry_mci_dead(Cache_entry* c){
	return (c->master_connection_id == (size_t)MCI_DEAD);
}