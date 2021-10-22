#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>

#define TEXT "Lorem ipsum dolor sit amet"
#define COUNT 10
#define BUFLEN 128
#define NOT_SHARED 0
#define SEM_PRT_NAME "/semParent"
#define SEM_CLD_NAME "/semChild"


pthread_t parent_thread_id;
sem_t * parent_sem, * child_sem;

void free_resources(){
    sem_unlink(SEM_PRT_NAME);
    sem_unlink(SEM_CLD_NAME);
}

char err_buf[BUFLEN];
void verify(int rc, const char* action){
    strerror_r(rc, err_buf, BUFLEN);
    if(rc != 0){
        fprintf(stderr, "Error %s: %s\n",
         action, err_buf);
        free_resources();
        pthread_exit(NULL);
    }
}
void verify_e(int rc, const char* action){
    if(rc < 0){
        perror(action);
        free_resources();
        pthread_exit(NULL);
    }
}

void * thread_body(void * unused) {
    char msg_buf[BUFLEN];
    pthread_t pid = pthread_self();
    sem_t * my_sem = (pid == parent_thread_id ? parent_sem : child_sem);
    sem_t * other_sem = (pid == parent_thread_id ? child_sem : parent_sem);

    verify_e(sprintf(msg_buf, "[%ld]: %s _\n", pid, TEXT), "sprintf");
    int len = strlen(msg_buf);

    for(int i = 0; i < COUNT; i++){
        msg_buf[len - 2] = '0' + i;
        sem_wait(my_sem);
        verify_e(write(1, msg_buf, len), "write");
        sem_post(other_sem);
    }
    return NULL;
}

int main() {
    pthread_t thread;

    parent_sem = sem_open(SEM_PRT_NAME, O_CREAT, 0600, 1);
    child_sem = sem_open(SEM_CLD_NAME, O_CREAT, 0600, 1);

    parent_thread_id = pthread_self();   
    printf("parent thread id: %ld\n", parent_thread_id);

    verify(pthread_create(&thread, NULL, thread_body, NULL), "pcreate");
    
    thread_body(NULL);

    void * retval;
    verify(pthread_join(thread, &retval), "pjoin");
    
    if (retval == PTHREAD_CANCELED) {
        printf("Thread was cancelled\n");
    } else { 
        printf("Thread was joined normally\n");
    }
    free_resources();
    pthread_exit(NULL);
}