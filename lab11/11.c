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

void * thread_body(void * id) {
    char msg_buf[BUFLEN];
    pthread_t pid = pthread_self();
    verify_e(sprintf(msg_buf, "[%ld]: %s _\n", pid, TEXT), "sprintf");
    int len = strlen(msg_buf);

    int cur_mutex = 0;
    verify(pthread_mutex_lock(mutex + 1), "mut start lock");
    if(isStarted){
        verify(pthread_mutex_unlock(mutex + 2), "mut start unlock");
    }
    for(int i = 0, j = 0; i < COUNT*MUTEXES; i++){
        verify(pthread_mutex_lock(mutex + cur_mutex), "mut lock");
        cur_mutex = (cur_mutex + 1) % MUTEXES;
        verify(pthread_mutex_unlock(mutex + cur_mutex), "mut unlock");

        if(cur_mutex == 1){
            msg_buf[len - 2] = '0' + j;
            j++;
            verify_e(write(1, msg_buf, len), "write");
            isStarted = 1;
        }
        cur_mutex = (cur_mutex + 1) % MUTEXES;
    }
    verify(pthread_mutex_unlock(mutex + 1), "mut end unlock");
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
    
    printf("parent thread id: %ld\n", pthread_self());
    // verify(pthread_mutex_lock(mutex + 0), "parent lock");

    verify(pthread_create(&thread, NULL, thread_body, NULL), "pcreate");
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
// #include <stdio.h>
// #include <unistd.h>
// #include <pthread.h>
// #include <stdlib.h>
// #include <string.h>

// #define MUTEXES_NUMBER 3
// #define ITERATIONS 10
// #define STATUS_SUCCESS 0
// #define STDOUT 0
// #define YES 1
// #define NO 0
// #define BUFFER_DEF_LENGTH 256

// char errorBuffer[BUFFER_DEF_LENGTH];
// pthread_mutex_t mutexes[MUTEXES_NUMBER];

// int hasItPrintedString = NO;

// void freeResources(int mutexesCount) {
//     for (int i = 0; i < mutexesCount; ++i) {
//         if (pthread_mutex_destroy(&mutexes[i]) != STATUS_SUCCESS) {
//             fprintf(stderr, "pthread_mutex_destroy problems");
//             pthread_exit(NULL);
//         }
//     }
// }

// void verifyPthreadFunctions(int returnCode, const char *functionName) {
//     strerror_r(returnCode, errorBuffer, BUFFER_DEF_LENGTH);
//     if (returnCode < STATUS_SUCCESS) {
//         fprintf(stderr, "Error %s: %s\n", functionName, errorBuffer);
//         freeResources(MUTEXES_NUMBER);
//         pthread_exit(NULL);
//     }
// }

// void *writeStrings(void *str) {
//     int currMutexIdx = 1;

//     verifyPthreadFunctions(pthread_mutex_lock(&mutexes[2]), "pthread_mutex_lock");
//     if (hasItPrintedString)
//         verifyPthreadFunctions(pthread_mutex_unlock(&mutexes[0]), "pthread_mutex_unlock");
//     for (int i = 0; i < ITERATIONS * MUTEXES_NUMBER; i++) {
//         verifyPthreadFunctions(pthread_mutex_lock(&mutexes[currMutexIdx]), "pthread_mutex_lock");
//         currMutexIdx = (currMutexIdx + 1) % MUTEXES_NUMBER;
//         verifyPthreadFunctions(pthread_mutex_unlock(&mutexes[currMutexIdx]), "pthread_mutex_unlock");
//         if (currMutexIdx == 2) {
//             fprintf(stdout, "%s\n",(const char *) str);
//             fflush(stdout);
//             hasItPrintedString = YES;
//         }
//         currMutexIdx = (currMutexIdx + 1) % MUTEXES_NUMBER;
//     }
//     verifyPthreadFunctions(pthread_mutex_unlock(&mutexes[2]), "pthread_mutex_unlock");

//     return NULL;
// }

// void initMutexes() {
//     pthread_mutexattr_t mattr;
//     pthread_mutexattr_init(&mattr);
//     verifyPthreadFunctions(pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK), "pthread_mutexattr_settype");
//     for (int i = 0; i < MUTEXES_NUMBER; ++i) {
//         if (pthread_mutex_init(&mutexes[i], &mattr) != STATUS_SUCCESS) {
//             fprintf(stderr, "pthread_mutex_init problems, thread didn't created");
//             freeResources(i + 1);
//         }
//     }
// }

// int main() {
//     pthread_t childrenThread;

//     initMutexes();

//     verifyPthreadFunctions(pthread_create(&childrenThread, NULL, writeStrings, (void *) "Children message"), "pthread_create");

// //    while (!printed) { sched_yield(); }

//     writeStrings((void *) "Parent message");

//     verifyPthreadFunctions(pthread_join(childrenThread, NULL), "pthread_join");

//     pthread_exit(EXIT_SUCCESS);
// }

// /* Fundament by Dmitrii V Irtegov */