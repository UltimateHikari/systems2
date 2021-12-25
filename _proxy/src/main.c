#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "dispatcher.h"
#include "verify.h"
#include "logger.h"
#include "prerror.h"
#define ARGS_MIN 3
#define PORT_MAX 65535
#define USAGE "Usage: local port[1024 - %d], class[32-33]; \n", PORT_MAX


int init_port(int argc, char** argv){
	int port;
	if(argc < ARGS_MIN || (port = atoi(argv[1])) < 1024 || port > PORT_MAX){
		LOG_INFO(USAGE);
		pthread_exit(NULL);
	}
	return port;
}

int init_class(int argc, char** argv){
	int class;
	if(argc < ARGS_MIN || ((class = atoi(argv[2])) != WTCLASS && class != MTCLASS)){
		LOG_INFO(USAGE);
		pthread_exit(NULL);
	}
	return class;
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
	init_signal();

	logger_initConsoleLogger(stderr);
	logger_setLevel(LogLevel_INFO);

	if((listener = init_listener(init_port(argc, argv))) != NULL){
		spin_listener(listener, init_class(argc, argv));
	}
	// only exit is by cleanup_listener inside;
	pthread_exit(NULL);
}