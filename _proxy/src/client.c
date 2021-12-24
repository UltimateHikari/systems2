#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "client.h"
#include "prerror.h"
#include "verify.h"
#include "server.h"
#include "cache.h"
#include "picohttpparser.h"
#include "logger.h"

// local declarations
int parse_into_request(Client_connection *c);
void free_as_request_owner(Client_connection *c, const char* error);
void log_request(int pret, size_t method_len, const char* method, size_t path_len, const char* path, int minor_version, size_t num_headers, struct phr_header *headers);
int client_register(Client_connection *c);
int client_read_n(Client_connection *c);
int client_proxy_n(Client_connection *c);
int proxy_read(Client_connection *c);
int proxy_write(Client_connection *c);
Chunk* init_current_chunk(Client_connection *c);
int check_if_done(Client_connection *c);

//cleanup

void * client_cleanup(void *raw_client){
	LOG_INFO("client_cleanup");
	RETURN_NULL_IF_NULL(raw_client);
	Client_connection ** client = (Client_connection**)raw_client;
	RETURN_NULL_IF_NULL(*client);
	free_connection(*client);
	*client = NULL;
	return NULL;
	// no pthread_exit because workers
}

// definitions

Request* make_request(){
	Request * r = (Request*)malloc(sizeof(Request));
	r->buflen = E_COMPARE;
	r->hostname = NULL;
	r->hostname_len = E_COMPARE;
	return r;
}

int free_request(Request *r){
	if(r == NULL){
		return E_DESTROY;
	}
	free(r);
	return S_DESTROY;
}

Client_connection *init_connection(int class, Dispatcher *d){
	Client_connection *c = (Client_connection*)malloc(sizeof(Client_connection));
	c->socket = NO_SOCK;
	c->cache = d->cache;
	c->entry = NULL;
	c->request = NULL;
	c->state = Parse;
	c->labclass = class;
	c->bytes_read = 0;
	c->c = NULL;
	c->d = (void*)d;
	c->current = NULL;
	c->is_registered = NOT_REGISTERED;
	c->debug_chunk_number=0;
	return c;
}

int free_connection(Client_connection *c){
	LOG_DEBUG("free_connection-call");
	if(c == NULL){
		return E_DESTROY;
	}
	unregister_connection(c);
	if(c->socket != NO_SOCK){
		close(c->socket);
	}
	if(c->c != NULL && c->c->state == Proxy){
		server_destroy_connection(c->c);
	}
	// entry and request belongs to cache
	free(c);
	return S_DESTROY;
}

void free_as_request_owner(Client_connection *c, const char* error){
	LOG_ERROR("Freeing as request owner:%s, fd:[%d]\n", error, c->socket);
	free_request(c->request);
	c->request = NULL;
	c->state = Done;
}

int register_connection(Client_connection *c){
	LOG_DEBUG("registering");
	if(c->is_registered == NOT_REGISTERED){
		if(verify(pthread_mutex_lock(&(c->entry->lag_lock)), 	\
			"lock for unreg", NO_CLEANUP, NULL) < 0){return E_REG;}; 
		c->entry->readers_amount++; 
		if(verify(pthread_mutex_unlock(&(c->entry->lag_lock)),
			"unlock for unreg", NO_CLEANUP, NULL) < 0){
				c->is_registered = IS_REGISTERED;
				return E_REG;
			};
		c->is_registered = IS_REGISTERED;
	}
	return S_REG;
}

int unregister_connection(Client_connection *c){
	LOG_DEBUG("unregistering");
	if(c->is_registered == IS_REGISTERED){
		if(verify(pthread_mutex_lock(&(c->entry->lag_lock)), 	\
			"lock for unreg", NO_CLEANUP, NULL) < 0){return E_REG;}; 
		c->entry->readers_amount--; 
		if(verify(pthread_mutex_unlock(&(c->entry->lag_lock)),
			"unlock for unreg", NO_CLEANUP, NULL) < 0){
				c->is_registered = NOT_REGISTERED;
				return E_REG;
			};
		c->is_registered = NOT_REGISTERED;
	}
	return S_REG;
}

void log_request(int pret, size_t method_len, const char* method, size_t path_len, const char* path, int minor_version, size_t num_headers, struct phr_header *headers){
	LOG_INFO("request is %d bytes long", pret);
	LOG_INFO("method is %.*s", (int)method_len, method);
	LOG_INFO("path is %.*s", (int)path_len, path);
	LOG_INFO("HTTP version is 1.%d", minor_version);
	LOG_INFO("headers:");
	for (size_t i = 0; i != num_headers; ++i) {
		LOG_INFO("%.*s: %.*s", (int)headers[i].name_len, headers[i].name,
			(int)headers[i].value_len, headers[i].value);
	}
}

//TODO(opt) remove extra headers parsing
int parse_into_request(Client_connection *c){
	LOG_DEBUG("parse_into_request-call");
	// brought mostly from example in github repo
	if(c == NULL || c->request == NULL || c->socket == NO_SOCK){
		return E_NULL;
	}
	char *buf = c->request->buf;
	const char *path;
	size_t path_len;
	const char *method;
	int pret, minor_version;
	struct phr_header headers[100];
	size_t buflen = 0, prevbuflen = 0, method_len, num_headers;
	ssize_t rret;

	while (1) {
			if(verify_e(rret = read(c->socket, buf + buflen, REQBUFSIZE - buflen), "read for parse", NO_CLEANUP, NULL) < 0){return E_PARSE;};
			prevbuflen = buflen;
			buflen += rret;
			c->request->buflen = buflen;

			num_headers = sizeof(headers) / sizeof(headers[0]);
			pret = phr_parse_request(buf, buflen, &method, &method_len, &path, &path_len,
					&minor_version, headers, &num_headers, prevbuflen);
			if (pret > 0)
					break; /* successfully parsed the request */
			else if (pret == -1)
					return E_PARSE;
			/* request is incomplete, continue the loop */
			/* also MT-safe */
			assert(pret == -2);
			if (buflen == REQBUFSIZE)
					return E_BIGREQ;
	}

	if(strncmp(method, "GET", 3) != 0){
		// only GET allowed
		return E_WMETHOD;
	}

	log_request(pret, method_len, method, path_len, path, minor_version, num_headers, headers);

	for (size_t i = 0; i != num_headers; ++i) {
		if(strncmp(headers[i].name, "Host", 4) == 0){
			LOG_DEBUG("Found host: %.*s\n", (int)headers[i].value_len, headers[i].value);
			c->request->hostname = (char*)malloc(headers[i].value_len);
			strncpy(c->request->hostname, headers[i].value, headers[i].value_len);
			c->request->hostname_len = headers[i].value_len;
			return S_PARSE;
		}
	}
	return E_PARSE;
}

int check_pointers_correctness(Client_connection *c, size_t ready, size_t read){
	if(ready < read){
		if(verify(pthread_mutex_unlock(&(c->entry->lag_lock)),								
		"unlock for wait", NO_CLEANUP, NULL) < 0){return E_WAIT;};
		panic_signal();
		LOG_ERROR("ready < read: %d < %d", ready, read);
		return E_WAIT;
	}
	return S_WAIT;
}

int wait_on_lock_cond(Client_connection *c, size_t *bytes_ready){
	pthread_mutex_t * lag_lock = &(c->entry->lag_lock);
	pthread_cond_t * lag_cond = &(c->entry->lag_cond);
	struct timespec ts;

	while(*bytes_ready == c->bytes_read){
		LOG_DEBUG("waiting for 1s: %d, %d", *bytes_ready, c->bytes_read);
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 1;
		int twret = pthread_cond_timedwait(lag_cond, lag_lock, &ts);

		if(twret == ETIMEDOUT){
			LOG_INFO("timed out");
			if(centry_mci_dead(c->entry)){
				LOG_INFO("creating new Server_connection");
				/*spin_server_connection(c);
			}
			return S_WAIT;*/
				LOG_INFO("actually no, Server_connection coming soon*");
			}
			return E_WAIT; // stub
		}

		if(twret < 0){
			return E_WAIT;
		}
		*bytes_ready = c->entry->bytes_ready;
	}
	return S_WAIT;
}

int wait_for_ready_bytes(Client_connection *c, size_t *bytes_ready){
	LOG_DEBUG("wait-for-call");
	pthread_mutex_t * lag_lock = &(c->entry->lag_lock);
	pthread_cond_t * lag_cond = &(c->entry->lag_cond);
	int rc;

	if(verify(pthread_mutex_lock(lag_lock), 		
		"lock for wait", NO_CLEANUP, NULL) < 0){return E_WAIT;};							
	*bytes_ready = c->entry->bytes_ready;
	
	if((rc = check_pointers_correctness(c, *bytes_ready, c->bytes_read)) != S_WAIT){
		return rc;
	}

	if((rc = wait_on_lock_cond(c, bytes_ready)) != S_WAIT){
		return rc;
	}

	if(verify(pthread_mutex_unlock(lag_lock),								
		"unlock for wait", NO_CLEANUP, NULL) < 0){return E_WAIT;};
	return S_WAIT;
}

#define BODY_FREE_CONNECTION							\
				free_connection(c);						\
				freed = 1;

void * client_body(void *raw_struct){
	LOG_DEBUG("client_body-call");
	// universal thread body
	if(raw_struct == NULL){
		return NULL;
	}
	Client_connection *c = (Client_connection*) raw_struct;

	int freed = 0, labclass = c->labclass;

	void * arg = (void*)&c;
	do{
		//TODO uncheck himself
		check_panic(client_cleanup, arg);
		switch(c->state){
			case Parse:
				if(client_register(c) != S_SEND){
					BODY_FREE_CONNECTION;
				}
				break;
			case Read:
				if(client_read_n(c) != S_SEND){
					BODY_FREE_CONNECTION;
				}
				break;
			case Proxy:
				if(client_proxy_n(c) != S_SEND){
					BODY_FREE_CONNECTION;
				}
				break;
			default:
				// if smh scheduled as done
				BODY_FREE_CONNECTION;
		}
	} while(!freed && labclass == MTCLASS);

	if(labclass == WTCLASS){
		//TODO: put ourselves into pending list;
	}
	return NULL; // implicit pthread_exit(NULL) if thread body
}

int client_register(Client_connection *c){
	LOG_DEBUG("client_register-call");
	c->request = make_request();

	if(parse_into_request(c) != S_PARSE){
		free_as_request_owner(c, "parse failed");
		return E_SEND;
	}

	if((c->entry = cache_find(c->cache, c->request)) == NULL){
		/* spin_server sets c->state to reading or proxying
			 depending on HTTP responce; also THERE entry is created */
		if(spin_server_connection(c) != S_CONNECT){
			free_as_request_owner(c, "server connect failed");
			return E_SEND;
		}
	} else {
		c->state = Read;
	}

	return S_SEND;
}

int client_read_n(Client_connection *c){
	LOG_DEBUG("read_n-call");
	// reads N chunks and returns
	//get position to read
	// TODO refactor to more constant reader registrarion; mb as function in cache
	size_t bytes_ready;
	if(wait_for_ready_bytes(c, &bytes_ready) != S_WAIT){
		LOG_ERROR("cond_timedwait failed");
		return E_SEND;
	}
	LOG_DEBUG("bytes_read, bytes_ready: %d, %d",c->bytes_read, bytes_ready);
	size_t bytes_read_before = c->bytes_read;
	for(int i = 0; (i < CHUNKS_TO_READ) && (c->bytes_read < bytes_ready); i++){
		size_t bytes_written = 0;
		if(c->current == NULL){
			init_current_chunk(c); // getting head;
		} else {
			c->current = c->current->next; //todo replace with on start of cycle
		}
		if(c->current == NULL){ //still NULL == error
			LOG_ERROR("chunks have less than bytes_ready - actual read");
			return E_SEND;
		}
		int wret;
		while( (wret = write(c->socket, c->current->data + bytes_written, c->current->size - bytes_written)) > 0){
			bytes_written += wret;
		}
		c->bytes_read += bytes_written;
		
		if(wret < 0 || bytes_written < c->current->size){
			LOG_ERROR("write error");
			return E_SEND;
		}
	}
	LOG_INFO("sent %d bytes, now %d", (c->bytes_read - bytes_read_before), c->bytes_read);
	return check_if_done(c);
}

int check_if_done(Client_connection *c){
	if(c->bytes_read == c->entry->bytes_expected){
		LOG_INFO("Client done reading");
		return E_SEND;
	}
	return S_SEND;
}

Chunk* init_current_chunk(Client_connection *c){
	c->current = c->entry->head;
	RETURN_NULL_IF_NULL(c->current);
	return c->current;
}

int client_proxy_n(Client_connection *c){
	LOG_DEBUG("proxy_n-call");
	Server_Connection *sc = c->c;

	if(sc->buflen > 0){
		if(proxy_write(c) == E_SEND){
			return E_SEND;
		}
	}

	for(int i = 0; (i < CHUNKS_TO_READ); i++){
		if(proxy_read(c) == E_SEND){
			return E_SEND;
		}
		if(c->state == Done){
			break;
		}
		if(proxy_write(c) == E_SEND){
			return E_SEND;
		}
	}
	return S_SEND;
}

int proxy_read(Client_connection *c){
	LOG_DEBUG("proxy_read-call");
	Server_Connection *sc = c->c;

	int wret = verify_e(read(sc->socket, sc->buf, REQBUFSIZE), "proxying read", NO_CLEANUP, NULL);

	if(wret < 0){
		LOG_ERROR("proxy_read error");
		return E_SEND;
	}
	sc->buflen = (size_t)wret;
	if(wret == 0){
		LOG_INFO("proxying connection: EOF reached");
		c->state = Done;
		return S_SEND;
	}
	return S_SEND;
}

int proxy_write(Client_connection *c){
	LOG_DEBUG("proxy_write-call");
	Server_Connection *sc = c->c;

	if(sc->buflen == 0){
		c->state = Done;
		return S_SEND;
	}

	int bytes_written = 0, wret;
	while((wret = write(c->socket, sc->buf + bytes_written, sc->buflen - bytes_written)) > 0){
		bytes_written += wret;
	}
	if(wret < 0){
		LOG_ERROR("proxy_write error");
		return E_SEND;
	}
	return S_SEND;
}