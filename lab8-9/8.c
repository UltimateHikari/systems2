#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define _GNU_SOURCE
#include <signal.h>
#include <unistd.h>
#include <string.h>

#define BUFLEN 128
#define MAXTHREADS 10000

#define NORMAL  1
#define SILENT  0
#define VERBOSE 2

#define num_steps 200000

int sig_flag = 0;
int isVerbose = NORMAL;
int thread_sig_iteration[MAXTHREADS];

char err_buf[BUFLEN];

pthread_mutex_t iter_mutex = PTHREAD_MUTEX_INITIALIZER;
int farthest_iter = 0;

void verify_r(int rc, const char* action){
    strerror_r(rc, err_buf, BUFLEN);
    if(rc != 0){
        fprintf(stderr, "Error %s: %s\n",
         action, err_buf);
        pthread_exit(NULL);
    }
}

void verify(int rc, const char* action){
    if(rc < 0){
        perror(action);
        pthread_exit(NULL);
    }
}

void setVerbosityLevel(const char* c){
    if(c[0] == 'v'){
        isVerbose = VERBOSE;
    }
    if(c[0] == 's'){
        isVerbose = SILENT;
    }
}

void quit_handler(int sig){
    //1 writer OR bunch of readers
    sig_flag = sig;
}

typedef struct {
    int n;
    int ofs;
} thread_data;

void * pibody(void * td){
    int n = ((thread_data*)td)->n;
    int i = ((thread_data*)td)->ofs;
    int iter = 0;
    int isDumped = 0, threadID = i;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGUSR1);
    verify_r(pthread_sigmask(SIGINT, &set, NULL), "block signal");

    double pi = 0.0;
    do{
        for (int j = 0; j < num_steps ; j++, i+=n){
            pi += 1.0/(i*4.0 + 1.0);
            pi -= 1.0/(i*4.0 + 3.0);
        }

        pthread_mutex_lock(&iter_mutex);
        if(!isDumped && sig_flag){
            isDumped = 1;
            thread_sig_iteration[threadID] = iter;
        }

        if(sig_flag && iter == farthest_iter){
            pthread_mutex_unlock(&iter_mutex);
            break;
        }else{
            iter++;
            if(iter > farthest_iter){
                farthest_iter = iter;
            }
        }
        pthread_mutex_unlock(&iter_mutex);
    }while(1);    

    double *res = (double*)malloc(sizeof(double));
    *res = pi;
    if(isVerbose == VERBOSE){
        printf("%f\n", pi); //order doesnt matter
    }
    pthread_exit(res);
}

void print_thread_stats(int nThreads){
    int min_iter = farthest_iter;
    double sum_lag = 0;
    for(int i = 0; i < nThreads; i++){
        min_iter = (min_iter > thread_sig_iteration[i] ? thread_sig_iteration[i] : min_iter);
        sum_lag += (double)(farthest_iter-thread_sig_iteration[i])/thread_sig_iteration[i];
    }
    printf("smallest iteration on signal read was %d of %d;\n", min_iter, farthest_iter);
    printf("max lag was %f, average lag was %f\n",
     (double)(farthest_iter - min_iter)/farthest_iter, sum_lag/nThreads);
}

int
main(int argc, char** argv) {
    if(argc < 2){
        printf("Usage: lab8 THREAD-COUNT [v|s]\n \
      Put v for verbosity and s for silence\n");
        exit(EXIT_FAILURE);
    }

    if(argc > 2){
        setVerbosityLevel(argv[2]);
    }

    if(atoi(argv[1]) > MAXTHREADS){
        printf("R u out of ur mind? Ask for less threads\n");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    sa.sa_handler = quit_handler;
    verify(sigaction(SIGINT, &sa, NULL), "sig_set");

    
    pthread_t thread[MAXTHREADS];
    thread_data data[MAXTHREADS];
    int nThreads = atoi(argv[1]);
    for(int i = 0; i < nThreads; i++){
        data[i].n = nThreads;
        data[i].ofs = i;
        //lowest chance to break for newly created thr on create exit
        verify(pthread_create(thread + i, NULL, pibody, (void *)(data + i)), "creating thread");
    }

    double pi = 0.0;
    double *piproxy;
    void *retval;
    for(int i = 0; i < atoi(argv[1]); i++){
        //if join fails, all remaining retvals leak
        verify(pthread_join(thread[i], (void**)&retval), "joining thread");
        piproxy = (double*)retval;
        pi += *piproxy;
        free(piproxy);
    }

    print_thread_stats(nThreads);

    pthread_mutex_destroy(&iter_mutex);

    if(isVerbose != SILENT){
        printf("pi done - %.15g \n", pi*4);
    } 
    
    pthread_exit(NULL);
}
