#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "dispatcher.h"
#include "verify.h"
#define ARGS_MIN 2
#define PORT_MAX 65535
#define USAGE "Usage: local port, 1024 - %d\n", PORT_MAX


int init_args(int argc, char** argv){
	int port;
	if(argc < ARGS_MIN || (port = atoi(argv[1])) < 1024 || port > PORT_MAX){
		printf(USAGE);
		pthread_exit(NULL);
	}
	return port;
}

void quit_handler(int unused){
	flag_signal();
}

void init_signal(){
	static struct sigaction act;
	act.sa_handler = quit_handler;
	sigaction(SIGINT, &act, NULL);
}

int main(int argc, char** argv){
	int listener_sc;
	//printf("%d,%d,%d", POLLERR, POLLHUP, POLLIN);

	int listener_port = init_args(argc, argv);
	init_signal();
	if(init_listener(&listener_sc, listener_port)){
		spin_listener(listener_sc);
	}
	flag_signal();
	join_threads();
	close(listener_sc);
	pthread_exit(NULL);
}