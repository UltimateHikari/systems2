#include <pthread.h>
#include "structs.h"

// pulls stuff from socket to cache

// func for starting server from client
int spin_server_connection(Client_connection *c);

int server_read_chunk(Server_Connection *c);
int server_destroy_connection(Server_Connection *c);