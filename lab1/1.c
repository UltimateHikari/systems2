#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#define TEXT "Lorem ipsum dolor sit amet, consectetur adipiscing elit\n"
#define COUNT 10

pid_t gettid(void);

void * thread_body(void * param) {
    pid_t pid = gettid();
    for(int i = 0; i < COUNT; i++){
        printf("[%d]: %d - %s", pid, i, TEXT);
    }
}

int main(int argc, char *argv[]) {
    pthread_t thread;


    if (pthread_create(&thread, NULL, thread_body, NULL) != 0) {
        perror("creating thread");
        exit(1);
    }

    thread_body(NULL);

    void * retval = NULL;
    if (pthread_join(thread, &retval) != 0){
        perror("joining thread");
        exit(1);
    }
    
    return (EXIT_SUCCESS);
}