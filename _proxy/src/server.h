#include <pthread.h>
#include "client.h"

typedef struct{
	int socket;
	Cache_entry *entry;
} Server_Connection;

// pulls stuff from socket to cache

int server_init_connection();
int spin_server_connection(Client_connection *c);
int server_read_chunk(Server_Connection *c);
int server_destroy_connection(Server_Connection *c);