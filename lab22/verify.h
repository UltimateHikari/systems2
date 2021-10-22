#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define BUFLEN 128
#define NO_CLEANUP NULL

int verify(int rc, const char* action, void(*free_resources)()){
    char err_buf[BUFLEN];
    printf("verifying %s\n", action); //debug for monitor
    strerror_r(rc, err_buf, BUFLEN);
    if(rc != 0){
        fprintf(stderr, "Error %s: %s\n",
         action, err_buf);
        if(free_resources != NO_CLEANUP) {free_resources();}
        pthread_exit(NULL);
    }
    return rc;
}
int verify_e(int rc, const char* action, void(*free_resources)()){
    if(rc < 0){
        perror(action);
        if(free_resources != NO_CLEANUP) {free_resources();}
        pthread_exit(NULL);
    }
    return rc;
}