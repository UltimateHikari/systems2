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

void * thread_body(void * param) {
    char msg_buf[BUFLEN];
    pthread_t pid = pthread_self();
    if(sprintf(msg_buf, "[%d]: %s _\n", pid, TEXT) < 0){
        perror("Sprintf");
        pthread_exit(NULL);
    }
    int len = strlen(msg_buf);
    for(int i = 0; i < COUNT; i++){
        msg_buf[len - 2] = '0' + i;
        if(write(1, msg_buf, len) < 0){
            perror("Write");
            pthread_exit(NULL);
        }
    }
}

int main(int argc, char *argv[]) {
    pthread_t thread;
    int rc;
    char err_buf[BUFLEN];

    if(argc < 2){
        printf("specify lab number [1/2]\n");
        pthread_exit(NULL); 
    }
    
    printf("parent thread id: %d\n", pthread_self());

    if ((rc = pthread_create(&thread, NULL, thread_body, NULL)) < 0) {
        fprintf(stderr, "Error creating thread: %s\n", strerror_r(rc, err_buf, BUFLEN));
        pthread_exit(NULL);
    }

    if(atoi(argv[1]) == 0){
        thread_body(NULL);
    }

    void * retval;
    if ((rc = pthread_join(thread, &retval)) < 0) {
        fprintf(stderr, "Error joining thread: %s\n", strerror_r(rc, err_buf, BUFLEN));
        pthread_exit(NULL);
    }
    
    if(atoi(argv[1]) == 1){
        thread_body(NULL);
    }
    if (retval == PTHREAD_CANCELED) {
        printf("Thread was cancelled\n");
    } else { 
        printf("Thread was joined normally\n");
    }
    pthread_exit(NULL);
}
