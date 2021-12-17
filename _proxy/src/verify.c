#include "verify.h"
#define BUFLEN 128

int exit_flag = 0;

void flag_signal(){
	exit_flag = 1;
}

int check_flag(){
	return exit_flag == 1;
}

void flog(const char* action){
	fprintf(stderr, "[%ld]: %s\n",
		pthread_self(), action);
}

void flog_e(const char* s1, const char* s2){
	fprintf(stderr, "[%ld] Error: %s - %s\n",
		pthread_self(), s1, s2);
}

int verify(int rc, const char* action, void(*free_resources)()){
	char err_buf[BUFLEN];
	//printf("verifying %s\n", action); //debug for monitor
	strerror_r(rc, err_buf, BUFLEN);
	if(rc != 0){
		flog_e(action, err_buf);
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