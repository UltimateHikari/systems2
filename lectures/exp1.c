#include <pthread.h>
#include <errno.h>
#include <unistd.h>

pthread_cond_t fileopen_cond=PTHREAD_COND_INITIALIZER;
pthread_mutex_t fileopen_mutex=PTHREAD_MUTEX_INITIALIZER;

int open_with_wait(const char *pathname, int flags, mode_t mode) {
    int code;

    pthread_mutex_lock(&fileopen_mutex);
    do {
        code=open(pathname, flags, mode);
        if (code < 0 && errno==EMFILE) {
            pthread_cond_wait(&fileopen_cond, &fileopen_mutex);
        }
    } while (code < 0 && errno==EMFILE);
    
    pthread_mutex_unlock(&fileopen_mutex);
    return code;
}

int close_with_wakeup(int handle) {
    int code;

    code=close(handle);
    pthread_cond_signal(&fileopen_cond);
    return code;
}