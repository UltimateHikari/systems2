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
void* worker_body(void *raw_worker);
void* dispatcher_body(void *raw_dispatcher);
int dispatcher_put(Dispatcher *d, void *conn);
int init_workers(Dispatcher *d);
int destroy_workers(Dispatcher *d);

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
	Dispatcher ** d = (Dispatcher**)raw_dispatcher;
	PRETURN_NULL_IF_NULL(*d);
	destroy_workers(*d);
	dispatcher_destroy(*d);
	*d = NULL;
	pthread_exit(NULL); 
}

int init_workers(Dispatcher *d){
	for(int i = 0; i < WORKERS_AMOUNT; i++){
		Worker * res = (Worker*)calloc(1, sizeof(Worker));
		res->state = Empty;
		res->d = d;
		sem_init(&(res->latch), 0, 0);	
		if(d->head_worker == NULL){
			d->head_worker = res;
		}else{
			res->next = d->head_worker;
			d->head_worker = res;
		}
		if(pthread_create(&(res->thread), NULL, worker_body,res) < 0){
			return E_INIT;
		}
	}
	return S_INIT;
}

int destroy_workers(Dispatcher *d){
	Worker *cur = d->head_worker;
	while(cur != NULL){
		pthread_join(cur->thread, NULL); //those pesky semaphores
		sem_destroy(&(cur->latch));
		free(cur);
		cur = cur->next;
	}
	return 0;
}

Dispatcher *init_dispatcher(Cache *cache, int labclass){
	Dispatcher *res = (Dispatcher*)malloc(sizeof(Dispatcher));
	RETURN_NULL_IF_NULL(res);
	res->labclass = labclass;
	res->cache = cache;
	res->isListenerAlive = 1;
	res->head_conn = NULL;
	res->last_conn = NULL;
	res->isStale = IsStale;

	void* arg = ((void*)&res);
	verify(pthread_mutex_init(&(res->dpatch_lock), NULL), "dpatch_lock init", dispatcher_cleanup, arg);
	verify(pthread_cond_init(&(res->dpatch_cond), NULL),  "dpatch_cond init", dispatcher_cleanup, arg);

	pthread_t unused;
	if(labclass == WTCLASS){
		verify(init_workers(res), "init workers", dispatcher_cleanup, arg);
		verify(pthread_create(&unused, NULL, dispatcher_body, res), "init thread", dispatcher_cleanup, arg);
	}
	return res;
}

int dispatcher_destroy(Dispatcher *d){
	if(d == NULL){
		return E_DESTROY;
	}
	cache_destroy(d->cache);
	panic_signal(); // dispatcher is key part
	destroy_workers(d);
	pthread_mutex_destroy(&(d->dpatch_lock));
	pthread_cond_destroy(&(d->dpatch_cond));
	free(d);
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

int spin_listener(Listener *listener, int labclass){
	LOG_INFO("Started spinning %d-class proxy:", labclass);
	LOG_INFO("Spinning cache...");
	Cache *cache = cache_init();
	if(cache == NULL){
		LOG_ERROR("cannot init cache, exiting");
		pthread_exit(NULL);
	}

	LOG_INFO("Spinning server...");
	Dispatcher *dispatcher = init_dispatcher(cache, labclass);
	if(dispatcher == NULL){
		LOG_ERROR("cannot init dispatcher, exiting");
		cache_destroy(cache);
		pthread_exit(NULL);
	}

	void* arg = (void*)&listener;
	while(1){
		check_panic(listener_cleanup, arg);
		Client_connection *cc = init_connection(dispatcher);
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
	Dispatcher *d = sc->d;
	void* arg = (void*)&d;
	if(d->labclass == MTCLASS){
		if(d->num_threads >= MAX_THREADS){
			return E_DISPATCH;
		}
		verify(pthread_create(
				d->threads + d->num_threads, NULL, server_body, (void *)sc),
				"create server", dispatcher_cleanup, arg);
		d->num_threads++;
	}
	if(d->labclass == WTCLASS && dispatcher_put(d, (void*)sc) != S_DISPATCH){
		return E_DISPATCH;
	}
	return S_DISPATCH;
}

int dispatcher_spin_client_reader(Client_connection *cc){
	LOG_DEBUG("dispatcher_spin_server_reader");
	Dispatcher *d = cc->d;
	void* arg = (void*)&d;
	if(d->labclass == MTCLASS){
		if(d->num_threads >= MAX_THREADS){
			return E_DISPATCH;
		}
		verify(pthread_create(
				d->threads + d->num_threads, NULL, client_body, (void *)cc),
				"create client", dispatcher_cleanup, arg);
		d->num_threads++;
	}
	if(d->labclass == WTCLASS && dispatcher_put(d, (void*)cc) != S_DISPATCH){
		return E_DISPATCH;
	}
	return S_DISPATCH;
}

D_entry *make_entry(void *conn){
	D_entry *res = (D_entry*)malloc(sizeof(D_entry));
	RETURN_NULL_IF_NULL(res);
	res->conn = conn;
	res->next = NULL;
	return res;
}

int destroy_entry(D_entry *d){
	if(d == NULL){
		return E_DESTROY;
	}
	free(d);
	return S_DESTROY;
}

int dispatcher_put(Dispatcher *d, void *conn){
	LOG_DEBUG("dispatcher-put");
	void* arg = (void*)&d;
	D_entry *entr = make_entry(conn);
	if(entr == NULL){
		LOG_DEBUG("entry not allocated");
		return E_DISPATCH;
	}

	if(verify(pthread_mutex_lock(&(d->dpatch_lock)), "put conn-lock", dispatcher_cleanup, arg) < 0){return E_DISPATCH;};
		if(d->head_conn == NULL){
			d->head_conn = entr;
			d->last_conn = entr;
		}else{
			d->last_conn->next = entr;
			d->last_conn = entr;
		}
	if(verify(pthread_cond_signal(&(d->dpatch_cond)), "put conn-signal", dispatcher_cleanup, arg) < 0){return E_DISPATCH;};
	d->isStale = Updated;
	if(verify(pthread_mutex_unlock(&(d->dpatch_lock)), "put conn-unlock", dispatcher_cleanup, arg) < 0){return E_DISPATCH;};
	return S_DISPATCH;
}


void* worker_body(void *raw_worker){
	LOG_DEBUG("worker_body");
	RETURN_NULL_IF_NULL(raw_worker);
	Worker *w = (Worker*)raw_worker;

	while(1){
		sem_wait(&(w->latch));
		check_panic(NO_CLEANUP, NULL); // all is covered by dispatcher

		void * res = w->body(w->arg);

		w->state = Empty;
		if(res != NULL){
			dispatcher_put(w->d, res);
		}

	}
	pthread_exit(NULL);
}

int dispatcher_wait(Dispatcher *d){
	struct timespec ts;
	pthread_mutex_t *dlock = &(d->dpatch_lock);
	pthread_cond_t *dcond = &(d->dpatch_cond);
	void* arg = (void*)&d;
	if(verify(pthread_mutex_lock(dlock), "try to sleep", dispatcher_cleanup, arg) < 0){return E_WAIT;};
		while (d->isStale == IsStale){
			LOG_DEBUG("waiting for 1s");
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 1;
			int twret = pthread_cond_timedwait(dcond, dlock, &ts);

			if(twret == ETIMEDOUT){
				LOG_INFO("timed out, dispatching anyway");
				return S_WAIT; // TODO: stub
			}

			if(twret < 0){
				dispatcher_cleanup(arg);
				return E_WAIT;
			}
		}
	if(verify(pthread_mutex_unlock(dlock), "untry to sleep", dispatcher_cleanup, arg) < 0){return E_WAIT;};
	return S_WAIT;
}

int try_dispatch(D_entry *e, Worker *cur_wrk){
	LOG_DEBUG("try-dispatch");
	while(cur_wrk != NULL){
		if(cur_wrk->state == Empty){
			cur_wrk->arg = e->conn;
			if(*((char*)(e->conn)) == CLIENT){
				LOG_DEBUG("deduced client");
				cur_wrk->body = client_body;
			}else{
				LOG_DEBUG("deduced server");
				cur_wrk->body = server_body;
			}
			cur_wrk->state = Working;
			sem_post(&(cur_wrk->latch));
			LOG_DEBUG("dispatched");
			return S_DISPATCH;
		}else{
			cur_wrk = cur_wrk->next;
		}
	}
	LOG_DEBUG("out of workers");
	return E_DISPATCH;
}

int check_head_conn(Dispatcher *d){
	void* arg = (void*)&d;
	pthread_mutex_t *dlock = &(d->dpatch_lock);
	int res = E_DISPATCH;
	if(verify(pthread_mutex_lock(dlock), "get ptrs", dispatcher_cleanup, arg) < 0){return E_DISPATCH;};
		if(d->head_conn != NULL){
			res = S_DISPATCH;
		}
	if(verify(pthread_mutex_unlock(dlock), "get ptrs-unlock", dispatcher_cleanup, arg) < 0){return E_DISPATCH;};
	return res;
}

int remove_head_conn(Dispatcher *d){
	void* arg = (void*)&d;
	pthread_mutex_t *dlock = &(d->dpatch_lock);
	if(verify(pthread_mutex_lock(dlock), "get ptrs", dispatcher_cleanup, arg) < 0){return E_DISPATCH;};
		D_entry *removed = d->head_conn;
		d->head_conn = d->head_conn->next;
	if(verify(pthread_mutex_unlock(dlock), "get ptrs-unlock", dispatcher_cleanup, arg) < 0){return E_DISPATCH;};
	destroy_entry(removed);
	return S_DISPATCH;
}

void perform_match(Dispatcher *d){
	LOG_DEBUG("perform-match");
	while(check_head_conn(d) == S_DISPATCH && try_dispatch(d->head_conn, d->head_worker)){
		remove_head_conn(d);
	}
}

void* dispatcher_body(void *raw_dispatcher){
	LOG_DEBUG("dispatcher-body");
	RETURN_NULL_IF_NULL(raw_dispatcher);
	Dispatcher *d = (Dispatcher*)raw_dispatcher;
	void* arg = (void*)&d;

	while (1){
		check_panic(dispatcher_cleanup, arg);
		if(dispatcher_wait(d) != S_WAIT){
			//shouldnt happen but anyway
			pthread_exit(NULL);
		}
		check_panic(dispatcher_cleanup, arg);
		perform_match(d);
	}
	pthread_exit(NULL);
}