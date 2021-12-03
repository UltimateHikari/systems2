#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define BUFLEN 128
#define BACKLOG 500
#define NO_CLEANUP NULL

#define RETURN_NULL_IF_NULL(a) if(a == NULL){ return NULL; }

int verify(int rc, const char* action, void(*free_resources)());
int verify_e(int rc, const char* action, void(*free_resources)());