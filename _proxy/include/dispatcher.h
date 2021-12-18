// MT/ worker-T type accept & dispatch
#include "structs.h"
#define BACKLOG 500

int init_listener(int *listener_socket, int listener_port);
int spin_listener(int listener_socket);
void join_threads();

int dispatcher_spin_server_reader(Server_Connection *sc);