#ifndef VERIFY_H
#define VERIFY_H


#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define NO_CLEANUP NULL

#define RETURN_NULL_IF_NULL(a) if(a == NULL){ return NULL; }
#define PRETURN_NULL_IF_NULL(a) if(a == NULL){ pthread_exit(NULL); }


void panic_signal();
int check_flag();
void check_panic(void*(*free_resources)(void *), void *arg);
int verify(int rc, const char* action, void*(*free_resources)(void * ), void *arg);
int verify_e(int rc, const char* action, void*(*free_resources)(void * ), void *arg);

#endif