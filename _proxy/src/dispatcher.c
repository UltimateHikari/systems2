#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "prerror.h"
#include "verify.h"

#include "dispatcher.h"
#include "cache.h"
#include "client.h"
#include "server.h"
#include "logger.h"

#define DEFAULT_PROTOCOL 0
#define NO_ADDR NULL

int dispatcher_spin_client_reader(Client_connection *sc);
int dispatcher_destroy(Dispatcher *dispatcher);

void* listener_cleanup(void *raw_listener){
	// dispatcher and existing connections can continue to listen
	LOG_INFO("listener_cleanup");
	PRETURN_NULL_IF_NULL(raw_listener);
	Listener ** listener = (Listener**)raw_listener;
	PRETURN_NULL_IF_NULL(*listener);
	if((*listener)->socket != NO_SOCK){
		LOG_DEBUG("closing accept socket");
		close((*listener)->socket);
	}
	free(*listener);
	*listener = NULL;
	pthread_exit(NULL); 
}

void* dispatcher_cleanup(void * raw_dispatcher){
	LOG_INFO("dispatcher_cleanup");
	panic_signal();
	PRETURN_NULL_IF_NULL(raw_dispatcher);
	Dispatcher ** dispatcher = (Dispatcher**)raw_dispatcher;
	PRETURN_NULL_IF_NULL(*dispatcher);
	dispatcher_destroy(*dispatcher);
	*dispatcher = NULL;
	pthread_exit(NULL); 
}

Dispatcher *init_dispatcher(Cache *cache){
	Dispatcher *dispatcher = (Dispatcher*)malloc(sizeof(Dispatcher));
	RETURN_NULL_IF_NULL(dispatcher);
	dispatcher->cache = cache;
	dispatcher->isListenerAlive = 1;
	dispatcher->clhead = NULL;
	dispatcher->schead = NULL;
	return dispatcher;
}

int dispatcher_destroy(Dispatcher *dispatcher){
	if(dispatcher == NULL){
		return E_DESTROY;
	}
	cache_destroy(dispatcher->cache);
	free(dispatcher);
	return S_DESTROY;
}

Listener* init_listener(int listener_port){
	struct sockaddr_in addr;

	Listener *listener = (Listener*)malloc(sizeof(Listener));
	RETURN_NULL_IF_NULL(listener);
	listener->socket = NO_SOCK;

	void* arg = (void*)&listener;
	listener->socket = verify_e(socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL),
			"ssock open", listener_cleanup, arg);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(listener_port);
	addr.sin_addr.s_addr = INADDR_ANY; //all interfaces


	verify_e(bind(listener->socket, (struct sockaddr*)&addr, sizeof(addr)), 
			"lsock bind", listener_cleanup, arg);

	verify_e(listen(listener->socket, BACKLOG), 
			"lsock listen", listener_cleanup, arg);
	return listener;
}

int spin_listener(Listener *listener){
	LOG_INFO("Spinning cache...");
	Cache *cache = cache_init();
	if(cache == NULL){
		LOG_ERROR("cannot init cache, exiting");
		pthread_exit(NULL);
	}

	LOG_INFO("Spinning server...");
	Dispatcher *dispatcher = init_dispatcher(cache);
	if(dispatcher == NULL){
		LOG_ERROR("cannot init dispatcher, exiting");
		cache_destroy(cache);
		pthread_exit(NULL);
	}

	void* arg = (void*)&listener;
	while(1){
		check_panic(listener_cleanup, arg);
		Client_connection *cc = init_connection(MTCLASS, dispatcher);
		cc->socket = verify_e(accept(listener->socket, NO_ADDR, NO_ADDR),
				"ssock accept", listener_cleanup, arg);
		LOG_INFO("accepted %d", cc->socket);
		if(dispatcher_spin_client_reader(cc) != S_DISPATCH){
			listener_cleanup(arg);
		}
	}
	return S_CONNECT;
}

void join_threads(Dispatcher *d){
	for(int i = 0; i < d->num_threads; i++){
		verify(pthread_join(d->threads[i], NULL),
				"join", NO_CLEANUP, NULL);
	}
}

int dispatcher_spin_server_reader(Server_Connection *sc){
	LOG_DEBUG("dispatcher_spin_server_reader");
	int labclass = sc->labclass;
	Dispatcher *d = (Dispatcher *)sc->d;
	void* arg = (void*)&d;
	if(labclass == MTCLASS){
		if(d->num_threads >= MAX_THREADS){
			return E_DISPATCH;
		}
		verify(pthread_create(
				d->threads + d->num_threads, NULL, server_body, (void *)sc),
				"create server", dispatcher_cleanup, arg);
		d->num_threads++;
	}
	if(labclass == WTCLASS){
		// TODO: put into queue & signal for dispatcher cond variable
	}
	return S_DISPATCH;
}

int dispatcher_spin_client_reader(Client_connection *cc){
	LOG_DEBUG("dispatcher_spin_server_reader");
	int labclass = cc->labclass;
	Dispatcher *d = (Dispatcher *)cc->d;
	void* arg = (void*)&d;
	if(labclass == MTCLASS){
		if(d->num_threads >= MAX_THREADS){
			return E_DISPATCH;
		}
		verify(pthread_create(
				d->threads + d->num_threads, NULL, client_body, (void *)cc),
				"create client", dispatcher_cleanup, arg);
		d->num_threads++;
	}
	if(labclass == WTCLASS){
		// TODO: put into queue & signal for dispatcher cond variable
	}
	return S_DISPATCH;
}