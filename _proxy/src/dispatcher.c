#include "dispatcher.h"

#define MAX_THREADS 100
#define DEFAULT_PROTOCOL 0
#define NO_ADDR NULL

#define CHECK_FLAG if(check_flag()){ return E_FLAG; } //TODO replace universal error

pthread_t threads[MAX_THREADS];
int num_threads = 0;


int init_listener(int *listener_socket, int listener_port){
  struct sockaddr_in addr;

	*listener_socket = verify_e(socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL),
	    "ssock open", flag_signal); CHECK_FLAG;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(listener_port);
	addr.sin_addr.s_addr = INADDR_ANY; //all interfaces


	verify_e(bind(*listener_socket, (struct sockaddr*)&addr, sizeof(addr)), 
	    "ssock bind", flag_signal); CHECK_FLAG;

	verify_e(listen(*listener_socket, BACKLOG), 
	    "ssock listen", flag_signal); CHECK_FLAG;
	return S_CONNECT;
}

int spin_listener(int listener_socket){
  printf("Spinning server...\n");

  while(!check_flag() && num_threads < MAX_THREADS){
  	Client_connection *cc = init_connection();
      cc->socket = verify_e(accept(listener_socket, NO_ADDR, NO_ADDR),
          "ssock accept", flag_signal); CHECK_FLAG;
      verify(pthread_create(
          threads + num_threads, NULL, client_body, (void *)cc),
          "create", flag_signal); CHECK_FLAG;
      num_threads++;
  }

  return S_CONNECT;
}

void join_threads(){
    for(int i = 0; i < num_threads; i++){
        verify(pthread_join(threads[i], NULL),
            "join", NO_CLEANUP);
    }
}