#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "prerror.h"
#include "verify.h"
#include "server.h"
#include "cache.h"

#define CHECK_FLAG if(check_flag()){ return E_FLAG; }
#define HTTP_PORT "80"


//local stuff

int server_parse_into_responce(Server_Connection * sc);
int server_connect(Client_connection *cl);
Server_Connection * server_init_connection(Client_connection * cl);

Server_Connection * server_init_connection(Client_connection * cl){
	flog("Server: init");
	Server_Connection *c = (Server_Connection*)malloc(sizeof(Server_Connection));
	cl->c = c;
	c->entry = cl->entry;
	if(server_connect(cl) < 0){
		free(c);
		cl->c = NULL;
		return NULL;
	}
	c->state = Parse;
	return c;
}

int server_connect(Client_connection *cl){
	flog("Server: connect");
	Server_Connection *c= cl->c;
	int sd;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};
	struct addrinfo *addrs, *addr;
	char *port_str = HTTP_PORT;
	char* hostname = cl->request->hostname;

	verify_e(getaddrinfo(hostname, port_str, &hints, &addrs), "resolving", flag_signal); CHECK_FLAG;

	for(addr = addrs; addr != NULL; addr = addr->ai_next){
		sd = verify_e(socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol), "trying socket", flag_signal); CHECK_FLAG;

		if (verify_e(connect(sd, addr->ai_addr, addr->ai_addrlen), "trying connect", NO_CLEANUP) == 0)
				break; //succesfully connected;

		close(sd);
		sd = -1;
	}
	if(sd == -1){
		return E_CONNECT;
	}
	c->socket = sd;
	printf("[%d]: connected to %s\n", sd, hostname);
	return S_CONNECT;
}

int spin_server_connection(Client_connection *c, size_t *bytes_expected){
	flog("Server: spin");
	Server_Connection *sc = server_init_connection(c);
	if(sc == NULL){
		return E_CONNECT;
	}

	if(server_send_request(c) == E_SEND){
		return E_SEND;
	}

	server_parse_into_responce(sc);
	// then find out if read or proxy c->state should be
	// and poke dispacher for registering if first
	// proxying is as a whole on client

	return S_CONNECT;
}

int server_send_request(Client_connection *c){
	flog("Server: send_request");
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

void * server_body(void *raw_struct){
	flog("Server: body");
	//TODO: stub
	return NULL;
}

int server_parse_into_responce(Server_Connection * sc){
	flog("Server: parse_into_responce");
	//TODO: stub// brought mostly from example in github repo
	// if(c == NULL || c->request == NULL || c->socket == NO_SOCK){
	// 	return E_NULL;
	// }
	// char *buf = c->request->buf;
	// const char *path = c->request->hostname;
	// size_t * path_len = &(c->request->hostname_len);
	// const char *method;
	// int pret, minor_version;
	// struct phr_header headers[100];
	// size_t buflen = 0, prevbuflen = 0, method_len, num_headers;
	// ssize_t rret;

	// while (1) {
	// 		verify_e(rret = read(c->socket, buf + buflen, REQBUFSIZE - buflen), "read for parse", flag_signal);
	// 		prevbuflen = buflen;
	// 		buflen += rret;

	// 		num_headers = sizeof(headers) / sizeof(headers[0]);
	// 		pret = phr_parse_request(buf, buflen, &method, &method_len, &path, path_len,
	// 				&minor_version, headers, &num_headers, prevbuflen);
	// 		if (pret > 0)
	// 				break; /* successfully parsed the request */
	// 		else if (pret == -1)
	// 				return E_PARSE;
	// 		/* request is incomplete, continue the loop */
	// 		/* also MT-safe */
	// 		assert(pret == -2);
	// 		if (buflen == REQBUFSIZE)
	// 				return E_BIGREQ;
	// }

	// if(strncmp(method, "GET", 3) != 0){
	// 	// only GET allowed
	// 	return E_WMETHOD;
	// }

	// log_request(pret, method_len, method, *path_len, path, minor_version, num_headers, headers);
	return 0;
}

int server_read_n(Client_connection *c);
int server_destroy_connection(Server_Connection *sc);