#include "verify.h"
#include "logger.h"
#define BUFLEN 128

int exit_flag = 0;

void flag_signal(){
	exit_flag = 1;
}

int check_flag(){
	return exit_flag == 1;
}

//TODO add chain of freers, as list and mutex for addition?

int verify(int rc, const char* action, void(*free_resources)()){
	char err_buf[BUFLEN];
	//printf("verifying %s\n", action); //debug for monitor
	strerror_r(rc, err_buf, BUFLEN);
	if(rc != 0){
		LOG_ERROR("%s - %s", action, err_buf);
		if(free_resources != NO_CLEANUP) {free_resources();}
		//pthread_exit(NULL);
	}
	return rc;
}

int verify_e(int rc, const char* action, void(*free_resources)()){
	if(rc < 0){
		perror(action);

		if(free_resources != NO_CLEANUP) {free_resources();}
		//pthread_exit(NULL);
	}
	return rc;
}