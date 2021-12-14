#include <pthread.h>

typedef struct{
	int socket;
	Cache_entry *entry;
} Server_Connection;

// pulls stuff from socket to cache

int server_init_connection();
int server_read_chunk();
int server_destroy_connection(Server_Connection *c);