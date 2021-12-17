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
#include "picohttpparser.h"


#define CHECK_FLAG if(check_flag()){ return E_FLAG; }
#define HTTP_PORT "80"


//local stuff

int server_parse_into_response(Server_Connection * sc, int *status, int *bytes_expected, char **mime, int *mime_len);
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
	c->buflen = E_COMPARE;
	return c;
}

int server_connect(Client_connection *cl){
	flog("Server: connect");
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
	fprintf(stderr, "Resolving %s ...\n", tmp_hostname);

	if((gret = getaddrinfo(
		tmp_hostname, port_str, &hints, &addrs)) != 0){
		fprintf(stderr, "%s\n", gai_strerror(gret));
		free(tmp_hostname);
		return E_RESOLVE;
	}
	free(tmp_hostname);

	for(addr = addrs; addr != NULL; addr = addr->ai_next){
		sd = verify_e(socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol), "trying socket", flag_signal); CHECK_FLAG;
		fprintf(stderr, "Connecting to %s...\n", addr->ai_addr->sa_data);
		if (verify_e(connect(sd, addr->ai_addr, addr->ai_addrlen), "trying connect", NO_CLEANUP) == 0)
				break; //succesfully connected;

		close(sd);
		sd = -1;
	}
	if(sd == -1){
		return E_CONNECT;
	}
	c->socket = sd;
	fprintf(stderr, "[%d]: connected to %.*s\n", sd, (int)cl->request->hostname_len, cl->request->hostname);
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

	int status, response_bytes_expected = E_COMPARE, mime_len = E_COMPARE;
	char *mime;
	server_parse_into_response(sc, &status, &response_bytes_expected, &mime, &mime_len);

	// TODO: then find out if read or proxy c->state should be
	// and poke dispacher for registering if first
	// proxying is as a whole on client

	if(status == 200){
		
	}

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

void log_response(int pret, int status, const char *msg, size_t msg_len, int minor_version, size_t num_headers, struct phr_header *headers){
	fprintf(stderr, "response is %d bytes long\n", pret);
	fprintf(stderr, "msg is %d %.*s\n", status, (int)msg_len, msg);
	fprintf(stderr, "HTTP version is 1.%d\n", minor_version);
	fprintf(stderr, "headers:\n");
	for (size_t i = 0; i != num_headers; ++i) {
		fprintf(stderr, "%.*s: %.*s\n", (int)headers[i].name_len, headers[i].name,
			(int)headers[i].value_len, headers[i].value);
	}
}

int server_parse_into_response(Server_Connection * sc, int *status, int *bytes_expected, char **mime, int *mime_len){
	flog("Server: parse_into_response");

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
			fprintf(stderr, "Found length: %.*s\n", (int)headers[i].value_len, headers[i].value);
			*bytes_expected = atoi(headers[i].value);
		}
	}

	for (size_t i = 0; i != num_headers; ++i) {
		if(strncmp(headers[i].name, "Content-Type", 12) == 0){
			fprintf(stderr, "Found mime: %.*s\n", (int)headers[i].value_len, headers[i].value);
			*mime = (char*)malloc(headers[i].value_len);
			strncpy(*mime, headers[i].value, headers[i].value_len);
			*mime_len = headers[i].value_len;
			return S_PARSE;
		}
	}

	return E_PARSE;
}

int server_read_n(Client_connection *c);
int server_destroy_connection(Server_Connection *sc);