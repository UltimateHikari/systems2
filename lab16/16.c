#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "verify.h"

#define TEXT "Lorem ipsum dolor sit amet"
#define COUNT 10
#define BUFLEN 128
#define NOT_SHARED 0
#define FORK_SUCCESS 0
#define SEM_PRT_NAME "/semParent"
#define SEM_CLD_NAME "/semChild"


pthread_t parent_thread_id;
sem_t * parent_sem, * child_sem;

void free_resources(){
    sem_close(parent_sem);
    sem_unlink(SEM_PRT_NAME);
    sem_close(child_sem);
    sem_unlink(SEM_CLD_NAME);
}

void * process_body(int pid) {
    char msg_buf[BUFLEN];
    sem_t * my_sem = (pid != 0 ? parent_sem : child_sem);
    sem_t * other_sem = (pid != 0 ? child_sem : parent_sem);

    verify_e(sprintf(msg_buf, "[%d]: %s _\n", pid, TEXT), "sprintf", free_resources);
    int len = strlen(msg_buf);

    for(int i = 0; i < COUNT; i++){
        msg_buf[len - 2] = '0' + i;
        sem_wait(my_sem);
        verify_e(write(1, msg_buf, len), "write", free_resources);
        sem_post(other_sem);
    }
    return NULL;
}

int main() {
    int wstatus, wres;
    pid_t pid = fork();

    if (pid < FORK_SUCCESS){
        perror("fork");
        exit(EXIT_FAILURE);
    }

    parent_sem = sem_open(SEM_PRT_NAME, O_CREAT, 0600, 1);
    child_sem = sem_open(SEM_CLD_NAME, O_CREAT, 0600, 0);

    process_body(pid);

    if(pid != 0){
    verify(wait(&wstatus), "wait", free_resources); //waitpid(-1,&wstatus) -/-1/0/>
 
    if(WIFEXITED(wstatus)){
        printf("child's exit status is: %d\n",
        WEXITSTATUS(wstatus));
    }else if (WIFSIGNALED(wstatus)){
        printf("signal is: %d\n",
        WTERMSIG(wstatus));
    }
    }

    free_resources();
    pthread_exit(NULL);
}