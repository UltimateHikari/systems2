// MT/ worker-T type accept & dispatch

#define BACKLOG 500

int init_listener(int *listener_socket, int listener_port);
int spin_listener(int listener_socket);
void join_threads();