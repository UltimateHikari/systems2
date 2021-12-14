#include "client.h"
#include "verify.h"
#include "picohttpparser.h"

// local declarations
int parse_request(Client_connection *c);

enum State{
	None,
	Parse,
	Read
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
	c->state = None;
	return c;
}

int free_connection(Client_connection *c){
	if(c == NULL){
		return E_DESTROY;
	}
	free(c);
	return S_DESTROY;
}

Request * parse_into_request(Client_connection *c){
	return NULL;
}

void * client_body(void *raw_struct){
	Client_connection *c = (Client_connection*) raw_struct;
	free((Client_connection*)raw_struct);

	printf("Hello thread\n");

	Request *r = parse_into_request(c);

	pthread_exit(NULL);
}
