#include <errno.h>
#include "verify.h"
#include "logger.h"
#define BUFLEN 128

int exit_flag = 0;

//global panic
void panic_signal(){
	exit_flag = 1;
}

int check_flag(){
	return exit_flag == 1;
}

void try_call(void*(*free_resources)(void *), void *arg){
	if(free_resources != NO_CLEANUP) {free_resources(arg);}
}

void check_panic(void*(*free_resources)(void *), void *arg){
	try_call(free_resources, arg);
	pthread_exit(NULL);
}

int verify_neq(int rc){
	return (rc != 0);
}

int verify_less(int rc){
	return (rc < 0);
}

int verify_common(int rc, const char* action, void*(*free_resources)(void *), void *arg, int(*condition)(int)){
	char err_buf[BUFLEN];
	strerror_r(rc, err_buf, BUFLEN);
	if(condition(rc)){
		LOG_ERROR("%s - %s", action, err_buf);
		try_call(free_resources, arg);
	}
	check_panic(free_resources, arg);
	return rc;
}

int verify(int rc, const char* action, void*(*free_resources)(void *), void *arg){
	return verify_common(rc, action, free_resources, arg, verify_neq);
}

//_e means errno 
int verify_e(int rc, const char* action, void*(*free_resources)(void *), void *arg){
	return verify_common(errno, action, free_resources, arg, verify_less);
}