#include "cache.h"

// Translation unit-local funcs
Cache_entry * centry_init();
int centry_destroy(Cache_entry * cache);

// Local  implementations

void cache_cleanup(){
	// not pthread_exit because of init in main thread
	exit(-1);
}

int mdata_is_equal(Request_metadata * a, Request_metadata *b){
	// TODO
	return 1;
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
	verify(pthread_mutex_init(&(res->collector_lock), NULL),  "c_lock init", cache_cleanup);

}

int cache_destroy(Cache * c){
	verify(pthread_mutex_destroy(&(c->structural_lock)), "s_lock destroy", cache_cleanup);
	verify(pthread_mutex_destroy(&(c->collector_lock)),  "c_lock destroy", cache_cleanup);
	free(c);
}

// returns entry on success 
// or NULL if not found
Cache_entry * cache_find(Cache * c, Request_metadata* mdata){
	RETURN_NULL_IF_NULL(c);
	Cache_entry *current = c->head;
	verify(pthread_mutex_lock(&(c->structural_lock)), "starting iter", NO_CLEANUP);
	while(current !=  NULL){
		if(mdata_is_equal(mdata, current->mdata)){
			verify(pthread_mutex_unlock(&(c->structural_lock)), "ending iter", NO_CLEANUP);
			return current;
		}
		current = current->next;
	}
	verify(pthread_mutex_unlock(&(c->structural_lock)), "ending iter", NO_CLEANUP);
}