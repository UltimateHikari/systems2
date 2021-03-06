#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "prerror.h"
#include "verify.h"
#include "server.h"
#include "cache.h"
#include "dispatcher.h"
#include "picohttpparser.h"
#include "logger.h"
#include "client.h"

#define HTTP_PORT "80"
//local stuff

Server_Connection * server_init_connection(Client_connection * cl);
int server_connect(Client_connection *cl);
int server_send_request(Client_connection *c);
void log_response(int pret, int status, const char *msg, size_t msg_len, int minor_version, size_t num_headers, struct phr_header *headers);
int server_parse_into_response(Server_Connection * sc, int *status, int *bytes_expected, char **mime, int *mime_len);
int choose_read_or_proxy(int status, int response_bytes_expected, char* mime, int mime_len, Client_connection *c);

void * server_cleanup(void *raw_server){
	LOG_INFO("server_cleanup");
	RETURN_NULL_IF_NULL(raw_server);
	Server_Connection ** server = (Server_Connection**)raw_server;
	RETURN_NULL_IF_NULL(*server);
	server_destroy_connection(*server);
	*server = NULL;
	return NULL;
	// no pthread_exit because workers
}

Server_Connection * server_init_connection(Client_connection * cl){
	LOG_DEBUG("server_init_connection-call");
	Server_Connection *c = (Server_Connection*)malloc(sizeof(Server_Connection));
	cl->c = c;
	c->type = SERVER;
	c->socket = NO_SOCK;
	c->entry = cl->entry;
	if(server_connect(cl) < 0){
		free(c);
		cl->c = NULL;
		return NULL;
	}
	c->state = Parse;
	c->buflen = E_COMPARE;
	c->d = cl->d;
	return c;
}

int server_connect(Client_connection *cl){
	LOG_DEBUG("server_connect-call");
	Server_Connection *c= cl->c;
	int sd, gret;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};
	struct addrinfo *addrs, *addr;
	char *port_str = HTTP_PORT;


	// +1 for \0
	char *tmp_hostname = (char*)calloc(cl->request->hostname_len + 1, sizeof(char));
	strncpy(tmp_hostname, cl->request->hostname, cl->request->hostname_len);
	LOG_INFO("Resolving %d: %s...", strlen(tmp_hostname), tmp_hostname);

	if((gret = getaddrinfo(
		tmp_hostname, port_str, &hints, &addrs)) != 0){
		LOG_INFO("%s", gai_strerror(gret));
		free(tmp_hostname);
		return E_RESOLVE;
	}
	free(tmp_hostname);

	for(addr = addrs; addr != NULL; addr = addr->ai_next){
		sd = verify_e(socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol), "trying socket", NO_CLEANUP, NULL);
		if(sd < 0){
			return E_CONNECT;
		}
		LOG_INFO("Connecting to %s...", addr->ai_addr->sa_data);
		if (verify_e(connect(sd, addr->ai_addr, addr->ai_addrlen), "trying connect", NO_CLEANUP, NULL) == 0)
				break; //succesfully connected;

		close(sd);
		sd = -1;
	}
	if(sd == -1){
		return E_CONNECT;
	}
	c->socket = sd;
	LOG_INFO("[%d]: connected to %.*s", sd, (int)cl->request->hostname_len, cl->request->hostname);
	return S_CONNECT;
}

int spin_server_connection(Client_connection *c){
	LOG_DEBUG("spin_server_connection-call");
	Server_Connection *sc = server_init_connection(c);
	if(sc == NULL){
		return E_CONNECT;
	}

	if(server_send_request(c) == E_SEND){
		return E_SEND;
	}

	int status, response_bytes_expected = E_COMPARE, mime_len = E_COMPARE;
	char *mime;
	server_parse_into_response(sc, &status, &response_bytes_expected, &mime, &mime_len);

	return choose_read_or_proxy(status, response_bytes_expected, mime, mime_len, c);
}

int choose_read_or_proxy(int status, int response_bytes_expected, char* mime, int mime_len, Client_connection *c){
	Server_Connection *sc = c->c;
	Cache_entry *entry;
	if(status == 200){
		// put in cache
		LOG_DEBUG("Trying Reading...");
		sc->state = Read;
		c->state = Read;
		if(response_bytes_expected == E_COMPARE){
			// no content-length was provided
			response_bytes_expected = BE_INF;
		}
		if((entry = cache_put(c->cache, response_bytes_expected, c->request, mime, mime_len)) == NULL){
			LOG_DEBUG("...Started Proxying");
			sc->state = Proxy;
			c->state = Proxy;
			return S_CONNECT;
		}
		sc->entry = entry;
		c->entry = entry;
		register_connection(c);
		if(dispatcher_spin_server_reader(sc) != S_DISPATCH){
			return E_CONNECT;
		}
		LOG_DEBUG("...Started Reading");
		return S_CONNECT;
	}
	LOG_DEBUG("...Started Proxying");
	sc->state = Proxy;
	c->state = Proxy;
	return S_CONNECT;
}

int server_send_request(Client_connection *c){
	LOG_DEBUG("send_request-call");
	ssize_t wret;
	Server_Connection *sc = c->c;
	char *buf = c->request->buf;
	size_t send_buflen = 0, req_buflen = c->request->buflen;
	while(send_buflen < req_buflen){
		if(verify_e(wret = write(sc->socket, buf + send_buflen, req_buflen - send_buflen), "write req", NO_CLEANUP, NULL) < 0){return E_SEND;};
		send_buflen += wret;
	}
	return S_SEND;
}

void log_response(int pret, int status, const char *msg, size_t msg_len, int minor_version, size_t num_headers, struct phr_header *headers){
	LOG_INFO("response is %d bytes long", pret);
	LOG_INFO("msg is %d %.*s", status, (int)msg_len, msg);
	LOG_INFO("HTTP version is 1.%d", minor_version);
	LOG_INFO("headers:");
	for (size_t i = 0; i != num_headers; ++i) {
		LOG_INFO("%.*s: %.*s", (int)headers[i].name_len, headers[i].name,
			(int)headers[i].value_len, headers[i].value);
	}
}

int server_parse_into_response(Server_Connection * sc, int *status, int *bytes_expected, char **mime, int *mime_len){
	LOG_DEBUG("parse_into_response-call");

	char *buf = sc->buf;
	const char *msg;
	int pret, minor_version;
	struct phr_header headers[100];
	size_t buflen = 0, prevbuflen = 0, msg_len, num_headers;
	ssize_t rret;

	while (1) {
			if(verify_e(rret = read(sc->socket, buf + buflen, REQBUFSIZE - buflen), "response read for parse", NO_CLEANUP, NULL) < 0){return E_PARSE;};
			prevbuflen = buflen;
			buflen += rret;
			sc->buflen = buflen;

			num_headers = sizeof(headers) / sizeof(headers[0]);
			pret = phr_parse_response(buf, buflen, &minor_version, status,
				&msg, &msg_len, headers, &num_headers, prevbuflen);
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

	log_response(pret, *status, msg, msg_len, minor_version, num_headers, headers);

	for (size_t i = 0; i != num_headers; ++i) {
		if(strncmp(headers[i].name, "Content-Length", 14) == 0){
			LOG_INFO("Found length: %.*s", (int)headers[i].value_len, headers[i].value);
			*bytes_expected = atoi(headers[i].value) + pret; //NOTE: Content-length + headers
		}
	}

	for (size_t i = 0; i != num_headers; ++i) {
		if(strncmp(headers[i].name, "Content-Type", 12) == 0){
			LOG_INFO("Found mime: %.*s", (int)headers[i].value_len, headers[i].value);
			*mime = (char*)malloc(headers[i].value_len);
			strncpy(*mime, headers[i].value, headers[i].value_len);
			*mime_len = headers[i].value_len;
			return S_PARSE;
		}
	}

	return E_PARSE;
}

void free_on_server_error(Server_Connection *sc, const char* error){
	// TODO free server_connection if proxying?
	LOG_ERROR("%s, fd:[%d]", error, sc->socket);
	centry_pop(sc->entry); //remove unwritten chunk for sake of next connections
	sc->state = Done;
}

void lag_broadcast(Cache_entry *c, size_t new_bytes){
	//LOG_DEBUG("my entry is %p", c);
	// extra bcasts in WTCLASS, but anyway..
	//TODO mb check error? not so relevant
	pthread_cond_t * lag_cond = &(c->lag_cond);
	pthread_mutex_t * lag_lock = &(c->lag_lock);
	verify(pthread_mutex_lock(lag_lock), "new_bytes update lock", NO_CLEANUP, NULL);
	c->bytes_ready += new_bytes;
	verify(pthread_mutex_unlock(lag_lock), "new_bytes update unlock", NO_CLEANUP, NULL);
	
	verify(pthread_cond_broadcast(lag_cond), "bcast lag cond", NO_CLEANUP, NULL);
	LOG_DEBUG("put %d bytes, now %d", new_bytes, c->bytes_ready);
}

#define BROADCAST_AND_RETURN(res) 					\
	lag_broadcast(sc->entry, read_on_iteration);	\
	return res;

int server_read_n(Server_Connection *sc){
	LOG_DEBUG("read_n-call");
	if(sc->entry == NULL){
		return E_READ;
	}

	Chunk * chunk;
	char * buf = NULL; // by default not putting buf into chunk
	size_t buflen = 0;

	if(sc->buflen > 0){
		// need to cache request from buf to cache;
		buflen = sc->buflen;
		buf = sc->buf;
		LOG_DEBUG("read_n - putting response: %d bytes", buflen);
		sc->buflen = 0;
	}

	size_t read_on_iteration = 0; //iteration means call
	if((chunk = centry_put(sc->entry, buf, buflen)) == NULL){
		BROADCAST_AND_RETURN(E_READ);
	}
	for(int i = 0; (i < CHUNKS_TO_READ); i++){
		if((chunk = centry_put(sc->entry, NULL, 0)) == NULL){
			LOG_ERROR("chunk creation");
			BROADCAST_AND_RETURN(E_READ);
		}
		if(check_flag()){
			LOG_ERROR("panic");
			return E_READ;
		}
		int wret = verify_e(read(sc->socket, chunk->data, REQBUFSIZE), "reading read", NO_CLEANUP, NULL);
		if(wret < 0){
			centry_pop(sc->entry);
			BROADCAST_AND_RETURN(E_READ);
		}
		chunk->size = (size_t)wret;
		read_on_iteration += (size_t)wret;
		
		if(wret == 0){
			LOG_INFO("reading read: EOF reached");
			sc->state = Done;
			BROADCAST_AND_RETURN(S_READ);
		}
	}
	BROADCAST_AND_RETURN(S_READ);
}

int server_destroy_connection(Server_Connection *sc){
	LOG_DEBUG("server_destroy_connection-call");
	if(sc == NULL){
		return E_NULL;
	}
	if(sc->socket != NO_SOCK){
		close(sc->socket);
	}
	//Cache_entry is handled by GC
	free(sc);
	return S_DESTROY;
}

void * server_body(void *raw_struct){
	LOG_DEBUG("server_body-call");
	RETURN_NULL_IF_NULL(raw_struct);

	Server_Connection *sc = (Server_Connection *)raw_struct;

	int labclass = sc->d->labclass;
	void * arg = (void*)&sc;
	do{
		check_panic(server_cleanup, arg);
		switch(sc->state){
			case Read:
				if(server_read_n(sc) != S_READ){
					LOG_ERROR("body freeing on error");
					server_destroy_connection(sc);
					return NULL;
				}
				break;
			default:
				LOG_INFO("freeing connection");
				server_destroy_connection(sc);
				return NULL;
		}
	}while(labclass == MTCLASS);

	return raw_struct;
}