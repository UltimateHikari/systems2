#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define _GNU_SOURCE
#include <errno.h>
#include <string.h>
#include <unistd.h>
#define TEXT "Lorem ipsum dolor sit amet"
#define COUNT 10
#define BUFLEN 128

pid_t gettid(void);

char err_buf[BUFLEN];

void verify(int rc, const char* action){
    if(rc < 0){
        fprintf(stderr, "Error %s: %s\n",
         action, strerror_r(rc, err_buf, BUFLEN));
        pthread_exit(NULL);
    }
}

void * thread_body(void * param) {
    char **param_a = (char**)param;
    while(*param_a != NULL){
        verify(write(1, *param_a, strlen(*param_a)), "write");
        param_a++;
    }
}

int main(int argc, char *argv[]) {
    pthread_t t[4];

    const char* param1[] = {"str11\n","str12\n","str13\n","str14\n", NULL};
    const char* param2[] = {"str21\n","str22\n","str23\n","str24\n", NULL};
    const char* param3[] = {"str31\n","str32\n","str33\n","str34\n", NULL};
    const char* param4[] = {"str41\n","str42\n","str43\n","str44\n", NULL};
    
    verify(pthread_create(t    , NULL, thread_body, param1),
            "creating thread1");
    verify(pthread_create(t + 1, NULL, thread_body, param2),
            "creating thread2");
    verify(pthread_create(t + 2, NULL, thread_body, param2),
            "creating thread3");
    verify(pthread_create(t + 3, NULL, thread_body, param3),
            "creating thread4");

    void * retval;
    for(int i = 0; i < 4; i++){
        verify(pthread_join(t[i], &retval), "joining thread");
        if (retval == PTHREAD_CANCELED) {
        printf("Thread %d was cancelled\n", i);
        } else { 
        printf("Thread %d was joined normally\n", i);
        }
    }

    
    pthread_exit(NULL);
}
