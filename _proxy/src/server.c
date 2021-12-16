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

int server_parse_header(Server_Connection * sc);
int server_connect(Server_Connection *c);
Server_Connection * server_init_connection(Cache_entry * centry);


Server_Connection * server_init_connection(Cache_entry * centry){
	Server_Connection *c = (Server_Connection*)malloc(sizeof(Server_Connection));
	c->entry = centry;
	if(server_connect(c) < 0){
		free(c);
		return NULL;
	}
	c->state = Parse;
	return c;
}

int server_connect(Server_Connection *c){
	int sd;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};
	struct addrinfo *addrs, *addr;
	char *port_str = HTTP_PORT;
	char* hostname = c->entry->mdata->hostname;

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

int spin_server_connection(Client_connection *c){
	Server_Connection *sc = server_init_connection(c->entry);
	if(c == NULL){
		return E_CONNECT;
	}

	server_parse_header(sc);
	// then find out if read or proxy c->state should be
	// and poke dispacher for registering if first
	// proxying is as a whole on client

	return S_CONNECT;
}

void * server_body(void *raw_struct){
	//TODO: stub
	return NULL;
}

int server_parse_header(Server_Connection * sc){
	//TODO: stub
	return 0;
}

int server_read_chunk(Server_Connection *c);
int server_destroy_connection(Server_Connection *c);