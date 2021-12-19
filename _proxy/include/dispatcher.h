// MT/ worker-T type accept & dispatch
#include "structs.h"
#define BACKLOG 500

Listener * init_listener(int listener_port);
int spin_listener(Listener *Listener);
void join_threads();

int dispatcher_spin_server_reader(Server_Connection *sc);