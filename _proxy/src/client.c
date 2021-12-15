#include "client.h"
#include "verify.h"
#include "picohttpparser.h"

// local declarations
int parse_request(Client_connection *c);

enum State{
	Parse,
	Read,
	Done
};

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

Client_connection *init_connection(){
	Client_connection *c = (Client_connection*)malloc(sizeof(Client_connection));
	c->socket = NO_SOCK;
	c->entry = NULL;
	c->request = NULL;
	c->state = Parse;
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

	//TODO fix tabs
	
	while (1) {
	    /* read the request */
			verify_e(rret = read(c->socket, buf + buflen, REQBUFSIZE - buflen), "read for parse", flag_signal);
	    prevbuflen = buflen;
	    buflen += rret;
	    /* parse the request */
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

	printf("request is %d bytes long\n", pret);
	printf("method is %.*s\n", (int)method_len, method);
	printf("path is %.*s\n", (int)path_len, path);
	printf("HTTP version is 1.%d\n", minor_version);
	printf("headers:\n");
	for (size_t i = 0; i != num_headers; ++i) {
	    printf("%.*s: %.*s\n", (int)headers[i].name_len, headers[i].name,
	           (int)headers[i].value_len, headers[i].value);
}
	return S_PARSE;
}

void * client_body(void *raw_struct){
	//TODO check errors here
	Client_connection *c = (Client_connection*) raw_struct;

	printf("Hello thread\n");

	c->request = make_request();
	int err = parse_into_request(c);

	if(err != S_PARSE){
		printf("Parse failed on %d\n", c->socket);
		free_request(c->request);
	}

	free_connection(c);
	pthread_exit(NULL);
}
