#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

#include "client.h"
#include "verify.h"
#include "prerror.h"
#include "cache.h"

// Translation unit-local funcs
void cache_cleanup();
int mdata_is_equal(Request * a, Request *b);
size_t curtime();

Cache_entry * centry_init(size_t bytes_expected, Request *mdata, size_t mci);
int centry_destroy(Cache_entry * c);
int chunk_destroy(Chunk *c);

int cache_garbage_check(Cache *c, size_t bytes_expected);
Cache_entry * cache_garbage_collect(Cache *c, size_t bytes_to_collect);
bool is_eligible_to_collect(Cache_entry *c);

// Local  implementations

void cache_cleanup(){
	// not pthread_exit because of init in main thread
	exit(-1);
}

int mdata_is_equal(Request * a, Request *b){
	if(a == NULL || b == NULL || a->hostname == NULL || b->hostname == NULL || a->hostname_len == E_COMPARE){
		return E_COMPARE;
	}
	return (strncmp(a->hostname, b->hostname, a->hostname_len) == 0);
}

size_t curtime(){
	struct timespec init;
	clock_gettime(CLOCK_MONOTONIC_RAW, &init);
	return init.tv_sec;
}

Cache_entry * centry_init(size_t bytes_expected, Request *mdata, size_t mci){
	Cache_entry * res = (Cache_entry*)malloc(sizeof(Cache_entry));
	RETURN_NULL_IF_NULL(res);

	res->bytes_ready = 0;
	res->bytes_expected = bytes_expected;
	res->mdata = mdata;
	res->mime = NULL;
	res->master_connection_id = mci;
	res->readers_amount = 1;
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

int cache_garbage_check(Cache *c, size_t bytes_expected){
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
	Cache_entry *current = c->head;
	Cache_entry *previous = NULL;
	Cache_entry *marked_head = NULL;
	Cache_entry *marked_last = NULL;

	size_t bytes_collected = 0;
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

	return marked_head;
}

bool is_eligible_to_collect(Cache_entry *c){
	// TODO maybe need to acquire collector lock
	return (c->readers_amount == 0);
}

// Header implementations

Cache * cache_init(){
	Cache * res = (Cache*)malloc(sizeof(Cache));
	RETURN_NULL_IF_NULL(res);

	res->chunk_size_bytes = 4096;
	res->max_size_bytes = 10*4096*1024;
	res->collect_threshold_percent = 50;
	res->current_expected_bytes = 0;
	res->head = NULL;
	res->last = NULL;
	res->marked = NULL;

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
Cache_entry * cache_find(Cache * c, Request* mdata){
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

Cache_entry * cache_put(Cache *c, size_t bytes_expected, Request *mdata, size_t mci){
	Cache_entry * newentry = centry_init(bytes_expected, mdata, mci);
	Cache_entry *marked, *next;
	bool is_nospace = false;

	verify(pthread_mutex_lock(&(c->structural_lock)), "adding entry", NO_CLEANUP);
		if(cache_garbage_check(c, bytes_expected) == E_NOSPACE){
			is_nospace = true;
		}
		if(c->head == NULL){
			c->head = newentry;
			c->last = newentry;
		}else{
			c->last->next = newentry;
			c->last = newentry;
		}
		c->current_expected_bytes = c->current_expected_bytes + bytes_expected;
	verify(pthread_mutex_unlock(&(c->structural_lock)), "adding entry", NO_CLEANUP);

	while(marked != NULL){
		next = marked-> next;
		centry_destroy(marked);
		marked = next;
	}

	if(is_nospace){
		centry_destroy(newentry);
		newentry = NULL;
	}

	return newentry;
}
