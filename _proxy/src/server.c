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


#define CHECK_FLAG if(check_flag()){ return E_FLAG; }
#define HTTP_PORT "80"


//local stuff

Server_Connection * server_init_connection(Client_connection * cl);
int server_connect(Client_connection *cl);
int server_send_request(Client_connection *c);
void log_response(int pret, int status, const char *msg, size_t msg_len, int minor_version, size_t num_headers, struct phr_header *headers);
int server_parse_into_response(Server_Connection * sc, int *status, int *bytes_expected, char **mime, int *mime_len);


Server_Connection * server_init_connection(Client_connection * cl){
	LOG_DEBUG("server_init_connection-call");
	Server_Connection *c = (Server_Connection*)malloc(sizeof(Server_Connection));
	cl->c = c;

	c->socket = NO_SOCK;
	c->entry = cl->entry;
	if(server_connect(cl) < 0){
		free(c);
		cl->c = NULL;
		return NULL;
	}
	c->state = Parse;
	c->labclass = cl->labclass;
	c->buflen = E_COMPARE;
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


	char *tmp_hostname = (char*)malloc(cl->request->hostname_len);
	strncpy(tmp_hostname, cl->request->hostname, cl->request->hostname_len);
	LOG_INFO("Resolving %s ...\n", tmp_hostname);

	if((gret = getaddrinfo(
		tmp_hostname, port_str, &hints, &addrs)) != 0){
		LOG_INFO("%s\n", gai_strerror(gret));
		free(tmp_hostname);
		return E_RESOLVE;
	}
	free(tmp_hostname);

	for(addr = addrs; addr != NULL; addr = addr->ai_next){
		sd = verify_e(socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol), "trying socket", flag_signal); CHECK_FLAG;
		LOG_INFO("Connecting to %s...\n", addr->ai_addr->sa_data);
		if (verify_e(connect(sd, addr->ai_addr, addr->ai_addrlen), "trying connect", NO_CLEANUP) == 0)
				break; //succesfully connected;

		close(sd);
		sd = -1;
	}
	if(sd == -1){
		return E_CONNECT;
	}
	c->socket = sd;
	LOG_INFO("[%d]: connected to %.*s\n", sd, (int)cl->request->hostname_len, cl->request->hostname);
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

	// TODO: then find out if read or proxy c->state should be
	// and poke dispacher for registering if read
	// proxying is as a whole on client

	if(status == 200){
		// put in cache
		LOG_DEBUG("Trying Reading...");
		sc->state = Read;
		c->state = Read;
		if(response_bytes_expected == E_COMPARE){
			// no content-length was provided
			response_bytes_expected = BE_INF;
		}
		if(cache_put(c->cache, response_bytes_expected, c->request, mime, mime_len) == NULL){
			LOG_DEBUG("...Started Proxying");
			sc->state = Proxy;
			c->state = Proxy;
			return S_CONNECT;
		}
		dispatcher_spin_server_reader(sc);
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
		verify_e(wret = write(sc->socket, buf + send_buflen, req_buflen - send_buflen), "write req", flag_signal); CHECK_FLAG;
		send_buflen += wret;
	}
	return S_SEND;
}

void log_response(int pret, int status, const char *msg, size_t msg_len, int minor_version, size_t num_headers, struct phr_header *headers){
	LOG_INFO("response is %d bytes long\n", pret);
	LOG_INFO("msg is %d %.*s\n", status, (int)msg_len, msg);
	LOG_INFO("HTTP version is 1.%d\n", minor_version);
	LOG_INFO("headers:\n");
	for (size_t i = 0; i != num_headers; ++i) {
		LOG_INFO("%.*s: %.*s\n", (int)headers[i].name_len, headers[i].name,
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
			verify_e(rret = read(sc->socket, buf + buflen, REQBUFSIZE - buflen), "response read for parse", flag_signal);
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
			LOG_INFO("Found length: %.*s\n", (int)headers[i].value_len, headers[i].value);
			*bytes_expected = atoi(headers[i].value);
		}
	}

	for (size_t i = 0; i != num_headers; ++i) {
		if(strncmp(headers[i].name, "Content-Type", 12) == 0){
			LOG_INFO("Found mime: %.*s\n", (int)headers[i].value_len, headers[i].value);
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
	LOG_ERROR("%s, fd:[%d]\n", error, sc->socket);
	centry_pop(sc->entry); //remove unwritten chunk for sake of next connections
	sc->state = Done;
}

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
		LOG_DEBUG("read_n - putting request");
		buflen = sc->buflen;
		buf = sc->buf;
	}

	LOG_DEBUG("read_n - trying chunk put");
	if((chunk = centry_put(sc->entry, buf, buflen)) == NULL){
		return E_READ;
	}

	for(int i = 0; (i < CHUNKS_TO_READ); i++){
		if((chunk = centry_put(sc->entry, NULL, 0)) == NULL){
			return E_READ;
		}
		int wret = verify_e(read(sc->socket, chunk->data, REQBUFSIZE), "reading read", NO_CLEANUP);;

		if(wret < 0){
			free_on_server_error(sc, "reading read");
			return E_SEND;
		}
		chunk->size = (size_t)wret;
		if(wret == 0){
			LOG_INFO("reading read: EOF reached");
			sc->state = Done;
		}
	}
	return S_READ;
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
	
	if(raw_struct == NULL){
		return NULL;
	}
	Server_Connection *sc = (Server_Connection *)raw_struct;

	int freed = 0, labclass = sc->labclass;
	do{
		switch(sc->state){
			case Read:
				if(server_read_n(sc) != S_READ){
					LOG_ERROR("body freeing on error");
					sc->state = Done;
					server_destroy_connection(sc);
					freed = 1;
				}
				break;
			default:
				LOG_INFO("freeing connection");
				sc->state = Done;
				server_destroy_connection(sc);
				freed = 1;
				break;
		}
		
	}while(!freed && labclass == MTCLASS);

	if(labclass == WTCLASS){
		// put in pending
	}
	return NULL;
}