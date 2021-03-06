#include <pthread.h>
#include "structs.h"

// pulls stuff from socket to cache

// func for starting server from client
int spin_server_connection(Client_connection *c);

int server_read_n(Server_Connection *sc);
void * server_body(void * raw_struct);

int server_destroy_connection(Server_Connection *sc);

void lag_broadcast(Cache_entry *c, size_t new_bytes);
