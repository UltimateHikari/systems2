#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define _GNU_SOURCE
#include <errno.h>
#include <string.h>
#define TEXT "Lorem ipsum dolor sit amet, consectetur adipiscing elit\n"
#define COUNT 10
#define BUFLEN 128

pid_t gettid(void);

void * thread_body(void * param) {
    pthread_t pid = pthread_self();
    for(int i = 0; i < COUNT; i++){
        printf("[%d]: %d - %s", pid, i, TEXT);
    }
}

int main(int argc, char *argv[]) {
    pthread_t thread;
    int rc;
    char err_buf[BUFLEN];

    if ((rc = pthread_create(&thread, NULL, thread_body, NULL)) < 0) {
        fprintf(stderr, "Error creating thread: %s\n", strerror_r(rc, err_buf, BUFLEN));
        exit(1);
    }

    thread_body(NULL);

    void * retval;
    if ((rc = pthread_join(thread, &retval)) < 0) {
        fprintf(stderr, "Error joining thread: %s\n", strerror_r(rc, err_buf, BUFLEN));
        exit(1);
    }

    if (retval == PTHREAD_CANCELED) {
        printf("Thread was cancelled\n");
    } else { 
        printf("Thread was joined normally\n");
    }
    return (EXIT_SUCCESS);
}
