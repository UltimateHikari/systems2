#include "cache.h"

// Translation unit-local funcs
Cache_entry * centry_init();
int centry_destroy(Cache_entry * c);
int chunk_destroy(Chunk *c);

// Local  implementations

void cache_cleanup(){
	// not pthread_exit because of init in main thread
	exit(-1);
}

int mdata_is_equal(Request_metadata * a, Request_metadata *b){
	// TODO: stub
	return 1;
}

size_t curtime(){
	struct timespec init;
	clock_gettime(CLOCK_MONOTONIC_RAW, &init);
	return init.tv_sec;
}

// Header implementations

Cache * cache_init(){
	Cache * res = (Cache*)malloc(sizeof(Cache));
	RETURN_NULL_IF_NULL(res);

	res->chunk_size_bytes = 4096;
	res->max_size_bytes = 4096000;
	res->collect_threshold_percent = 50;
	res->head = NULL;

	verify(pthread_mutex_init(&(res->structural_lock), NULL), "s_lock init", cache_cleanup);
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
	verify(pthread_mutex_destroy(&(c->structural_lock)), "s_lock destroy", cache_cleanup);
	free(c);

	return 0;
}

// returns entry on success 
// or NULL if not found
Cache_entry * cache_find(Cache * c, Request_metadata* mdata){
	RETURN_NULL_IF_NULL(c);
	Cache_entry *current = c->head;

	verify(pthread_mutex_lock(&(c->structural_lock)), "find starting iter", NO_CLEANUP);
		// TODO critical section can be reduced with snapshotting?
		// or collector should be kept out from ruining snapshot?
		while(current !=  NULL){
			if(mdata_is_equal(mdata, current->mdata)){
				verify(pthread_mutex_unlock(&(c->structural_lock)), "found ending iter", NO_CLEANUP);
				return current;
			}
			current = current->next;
		}
	verify(pthread_mutex_unlock(&(c->structural_lock)), "not found ending iter", NO_CLEANUP);
	return NULL;
}

Cache_entry * cache_put(Cache *c, size_t bytes_expected, Request_metadata *mdata, size_t mci){
	Cache_entry * newentry = centry_init(bytes_expected, mdata, mci);

	verify(pthread_mutex_lock(&(c->structural_lock)), "adding entry", NO_CLEANUP);
		newentry->next = c->head;
		c->head = newentry;
	verify(pthread_mutex_unlock(&(c->structural_lock)), "adding entry", NO_CLEANUP);
	return newentry;
}

Cache_entry * centry_init(size_t bytes_expected, Request_metadata *mdata, size_t mci){
	Cache_entry * res = (Cache_entry*)malloc(sizeof(Cache_entry));
	RETURN_NULL_IF_NULL(res);

	res->bytes_ready = 0;
	res->bytes_expected = bytes_expected;
	res->mdata = mdata;
	res->master_connection_id = mci;
	res->readers_amount = 1;
	res->last_access_ms = curtime();
	res->head = NULL;
	res->next = NULL;

	verify(pthread_mutex_init(&(res->lag_lock), NULL), "l_lock init", cache_cleanup);
	verify(pthread_cond_init(&(res->lag_cond), NULL),  "l_cond init", cache_cleanup);
	return res;
}

int centry_destroy(Cache_entry * c){
	if(c == NULL){
		return E_DESTROY;
	}

	Chunk *current = c->head;
	Chunk *next;
	while(current != NULL){
		next = current->next;
		chunk_destroy(current);
		current = next;
	}
	verify(pthread_mutex_destroy(&(c->lag_lock)), "l_lock destroy", cache_cleanup);
	verify(pthread_cond_destroy(&(c->lag_cond)),  "c_lock destroy", cache_cleanup);
	free(c);

	return S_DESTROY;
}

int chunk_destroy(Chunk *c){
	if(c == NULL){
		return E_DESTROY;
	}
	free(c->data);
	free(c);
	return S_DESTROY;
}