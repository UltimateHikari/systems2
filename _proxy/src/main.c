#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "dispatcher.h"
#include "verify.h"
#include "logger.h"
#define ARGS_MIN 2
#define PORT_MAX 65535
#define USAGE "Usage: local port, 1024 - %d\n", PORT_MAX


int init_args(int argc, char** argv){
	int port;
	if(argc < ARGS_MIN || (port = atoi(argv[1])) < 1024 || port > PORT_MAX){
		LOG_INFO(USAGE);
		pthread_exit(NULL);
	}
	return port;
}

void quit_handler(int unused){
	panic_signal();
}

void init_signal(){
	static struct sigaction act;
	act.sa_handler = quit_handler;
	sigaction(SIGINT, &act, NULL);
}

int main(int argc, char** argv){
	Listener *listener;
	int listener_port = init_args(argc, argv);
	init_signal();

	logger_initConsoleLogger(stderr);
	logger_setLevel(LogLevel_DEBUG);

	if((listener = init_listener(listener_port)) != NULL){
		spin_listener(listener);
	}
	// only exit is by cleanup_listener inside;
	pthread_exit(NULL);
}