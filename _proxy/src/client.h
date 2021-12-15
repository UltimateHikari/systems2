#ifndef CLIENT_H
#define CLIENT_H

#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include "prerror.h"
#include "cache.h"

#define PARSE 0
#define READ 1
#define REQBUFSIZE 1024

/**
 * Client_connection:
 * gives request
 * makes decision about server_connection
 * connects to cache and starts reading
 */

enum State{
	Parse,
	Read, // from cache
	Proxy,
	Done
};


typedef struct {
	char buf[REQBUFSIZE];
	char *hostname;
} Request;

typedef struct{
	int socket;
	Cache *cache;
	Cache_entry *entry;
	Request *request;
	int state;
	int labclass;
} Client_connection;

Request *make_request();
int free_request(Request *r);

Client_connection *init_connection();
int free_connection(Client_connection *c);

void * client_body(void *raw_struct);

#endif