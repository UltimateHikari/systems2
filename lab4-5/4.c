#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define _GNU_SOURCE
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define TEXT "Lorem ipsum dolor sit amet"
#define COUNT 20
#define WAIT_TIME 2
#define BUFLEN 128
#define NO_EXEC

char err_buf[BUFLEN];

void verify(int rc, const char* action){
    strerror_r(rc, err_buf, BUFLEN);
    if(rc < 0){
        fprintf(stderr, "Error %s: %s\n",
         action, err_buf);
        pthread_exit(NULL);
    }
}

void cleanup_handler(void * unused){
    char* note = "Child: i was cancelled\n";
    verify(write(1, note, strlen(note)), "note");
}

void * thread_body(void * unused) {
    time_t start, curr;
    int cnt = 0;
    /**
     * why r u ~~gae~~ macro
     * i cant check lab num in runtime bc of u
     */
    pthread_cleanup_push(cleanup_handler, NULL);

    char msg_buf[BUFLEN];
    pthread_t pid = pthread_self();

    verify(sprintf(msg_buf, "[%ld]: %s _\n", pid, TEXT), "sprintf");

    curr = start = time(NULL);

    while (1) {
        if (curr < time(NULL)) {
            curr = time(NULL);
            printf("cnt = %d\n", cnt);  /* A cancellation point */
            cnt++;
        }
        sleep(1);
    }

    int len = strlen(msg_buf);
    for(int i = 0; i < COUNT; i++){
        pthread_testcancel();
        msg_buf[len - 2] = '0' + i;
        verify(write(1, msg_buf, len) < 0, "write");
    }
    // u too buddy
    pthread_cleanup_pop(0);
}

int main(int argc, char *argv[]) {
    pthread_t thread;

    // if(argc < 2){
    //     printf("specify lab number [4/5]\n");
    //     pthread_exit(NULL); 
    // }
    
    verify(pthread_create(&thread, NULL, thread_body, argv[1]), "creating thread");

    sleep(WAIT_TIME); //task-specific

    void * retval;
    verify(pthread_cancel(thread), "cancel");
    verify(pthread_join(thread, &retval), "join");

    if (retval == PTHREAD_CANCELED) {
        printf("Thread was cancelled\n");
    } else { 
        printf("Thread was joined normally\n");
    }
    pthread_exit(NULL);
}
