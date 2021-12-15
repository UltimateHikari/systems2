#include "client.h"
#include "verify.h"
#include "server.h"
#include "picohttpparser.h"
#include <string.h>

// local declarations
int parse_into_request(Client_connection *c);
void free_on_error(Client_connection *c, const char* error);
void log_request(int pret, size_t method_len, const char* method, size_t path_len, const char* path, int minor_version, size_t num_headers, struct phr_header *headers);
void client_register(Client_connection *c);
void client_read(Client_connection *c);

// definitions

Request* make_request(){
	Request * r = (Request*)malloc(sizeof(Request));
	r->hostname = NULL;
	return r;
}

int free_request(Request *r){
	if(r == NULL){
		return E_DESTROY;
	}
	if(r->hostname != NULL){
		free(r->hostname);
	}
	free(r);
	return S_DESTROY;
}

Client_connection *init_connection(int class, Cache *cache){
	Client_connection *c = (Client_connection*)malloc(sizeof(Client_connection));
	c->socket = NO_SOCK;
	c->cache = cache;
	c->entry = NULL;
	c->request = NULL;
	c->state = Parse;
	c->labclass = class;
	return c;
}

int free_connection(Client_connection *c){
	if(c == NULL){
		return E_DESTROY;
	}
	if(c->socket != NO_SOCK){
		close(c->socket);
	}
	// entry and request belongs to cache
	free(c);
	return S_DESTROY;
}

void free_on_error(Client_connection *c, const char* error){
	fprintf("Error: client connection: %s %d\n", error, c->socket);
	free_request(c->request);
	c->state = Done;
}

void log_request(int pret, size_t method_len, const char* method, size_t path_len, const char* path, int minor_version, size_t num_headers, struct phr_header *headers){
	printf("request is %d bytes long\n", pret);
	printf("method is %.*s\n", (int)method_len, method);
	printf("path is %.*s\n", (int)path_len, path);
	printf("HTTP version is 1.%d\n", minor_version);
	printf("headers:\n");
	for (size_t i = 0; i != num_headers; ++i) {
	    printf("%.*s: %.*s\n", (int)headers[i].name_len, headers[i].name,
	           (int)headers[i].value_len, headers[i].value);
}
}

//TODO(opt) remove extra headers parsing
//TODO fix tabs	
int parse_into_request(Client_connection *c){
	// brought mostly from example in github repo
	if(c == NULL || c->request == NULL || c->socket == NO_SOCK){
		return E_NULL;
	}
	char *buf = c->request->buf;
	const char *method, *path;
	int pret, minor_version;
	struct phr_header headers[100];
	size_t buflen = 0, prevbuflen = 0, method_len, path_len, num_headers;
	ssize_t rret;

	while (1) {
			verify_e(rret = read(c->socket, buf + buflen, REQBUFSIZE - buflen), "read for parse", flag_signal);
	    prevbuflen = buflen;
	    buflen += rret;

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

	return S_PARSE;
}

//TODO error verification? or better in connection struct?
void * client_body(void *raw_struct){
	// universal thread body
	if(raw_struct == NULL){
		pthread_exit(NULL);
	}
	Client_connection *c = (Client_connection*) raw_struct;

	switch(c->state){
		case Parse:
			client_register(c);
			// no break, should try to read afterwards;
		case Read:
			client_read(c);
			break;
		default:
			// if smh scheduled as done
			free_connection(c);
			pthread_exit(NULL);
	}
}

void client_register(Client_connection *c){
	c->request = make_request();

	if(parse_into_request(c) != S_PARSE){
		free_on_error(c, "parse failed");
		return;
	}

	if((c->entry = cache_find(c->cache, c->request)) == NULL){
		/* spin_server sets c->state to reading or proxying
			 depending on HTTP responce */
		if(spin_server_connection(c) != S_CONNECT){
			free_on_error(c, "server connect failed");
			return;
		}
	} else {
		c->state = Read;
	}

}

void client_read(Client_connection *c){
	//TODO: stub
	// reads N chunks and returns
	if(c->labclass == WTCLASS){
		return;
	}
	if(c->labclass == MTCLASS){
		// hol'up on cond var in centry
	}
}

