#ifndef VERIFY_H
#define VERIFY_H


#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define NO_CLEANUP NULL

#define RETURN_NULL_IF_NULL(a) if(a == NULL){ return NULL; }


void flag_signal();
int check_flag();
int verify(int rc, const char* action, void(*free_resources)());
int verify_e(int rc, const char* action, void(*free_resources)());

#endif