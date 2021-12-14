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

Request* make_request();
int free_request(Request *r);

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

void * client_body(void *raw_struct){
	printf("Hello thread\n");
	pthread_exit(NULL);
}