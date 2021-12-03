#include <pthread.h>
#include <stdio.h>
#include "cache.h"

int main(){
	Cache *cache = cache_init();

	cache_destroy(cache);
	pthread_exit(NULL);
}