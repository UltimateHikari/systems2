// MT/ worker-T type accept & dispatch
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "prerror.h"
#include "verify.h"
#include "client.h"


#define BACKLOG 500

int init_listener(int *listener_socket, int listener_port);
int spin_listener(int listener_socket);
void join_threads();