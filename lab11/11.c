#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <errno.h>
#include <string.h>
#include <unistd.h>
#define TEXT "Lorem ipsum dolor sit amet"
#define COUNT 10
#define BUFLEN 128
#define MUTEXES 3
#define CUR_INC (cur_mutex + 1) % MUTEXES

pthread_mutex_t mutex[MUTEXES];
int isStarted = 0;

pthread_t parent_thread_id;

char err_buf[BUFLEN];
void verify(int rc, const char* action){
    strerror_r(rc, err_buf, BUFLEN);
    if(rc != 0){
        fprintf(stderr, "Error %s: %s\n",
         action, err_buf);
        pthread_exit(NULL);
    }
}
void verify_e(int rc, const char* action){
    if(rc < 0){
        perror(action);
        pthread_exit(NULL);
    }
}

void * thread_body(void * unused) {
    char msg_buf[BUFLEN];
    int cur_mutex = 0, next_mutex;
    pthread_t pid = pthread_self();
    if(parent_thread_id != pid){
        cur_mutex = 2;
        verify(pthread_mutex_lock(mutex + cur_mutex), "child start lock");
    }

    verify_e(sprintf(msg_buf, "[%ld]: %s _\n", pid, TEXT), "sprintf");
    int len = strlen(msg_buf);

    for(int i = 0; i < COUNT; i++){
        next_mutex = (cur_mutex + 1) % MUTEXES;
        verify(pthread_mutex_lock(mutex + next_mutex), "mut lock");

        msg_buf[len - 2] = '0' + i;
        verify_e(write(1, msg_buf, len), "write");

        verify(pthread_mutex_unlock(mutex + cur_mutex), "mut unlock");
        cur_mutex = next_mutex;
    }
    verify(pthread_mutex_unlock(mutex + cur_mutex), "mut unlock");
    return NULL;
}

int main() {
    pthread_t thread;

    pthread_mutexattr_t mattr;
    verify(pthread_mutexattr_init(&mattr), "attr init");
    verify(pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK), "attr set");

    for(int i = 0; i < MUTEXES; i++){
        verify(pthread_mutex_init(mutex + i, &mattr), "setattr");
    }

    parent_thread_id = pthread_self();   
    printf("parent thread id: %ld\n", parent_thread_id);

    verify(pthread_mutex_lock(mutex), "parent start lock");
    verify(pthread_create(&thread, NULL, thread_body, NULL), "pcreate");
    
    while(pthread_mutex_trylock(mutex + 2) == 0){
        pthread_mutex_unlock(mutex + 2);
        sched_yield();
    }
    
    thread_body(NULL);

    void * retval;
    verify(pthread_join(thread, &retval), "pjoin");
    
    if (retval == PTHREAD_CANCELED) {
        printf("Thread was cancelled\n");
    } else { 
        printf("Thread was joined normally\n");
    }
    pthread_exit(NULL);
}