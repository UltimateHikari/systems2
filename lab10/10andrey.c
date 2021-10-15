#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>

#define PHILO 5
#define DELAY 30000
#define FOOD 50
#define VERBOSE 1
#define SILENT  0
#define SP_POSIX 1
#define SP_THREAD 0

pthread_mutex_t forks[PHILO];
pthread_t phils[PHILO];
void *philosopher (void *id);
int food_on_table ();
void get_fork (int, int, char *);
void get_forks(int, int, int);
void down_forks (int, int);
pthread_mutex_t foodlock;

int sleep_seconds = 0;
int verbosity = SILENT;
int policy = SP_POSIX;
int max_priority;

int main (int argn,
      char **argv)
{
    int i;

    if (argn == 2)
        //sleep_seconds = atoi (argv[1]);
        policy = atoi(argv[1]);
    

    pthread_mutex_init (&foodlock, NULL);
    for (i = 0; i < PHILO; i++)
        pthread_mutex_init (&forks[i], NULL);
    for (i = 0; i < PHILO; i++)
        pthread_create (&phils[i], NULL, philosopher, (void *) &i);
    for (i = 0; i < PHILO; i++)
        pthread_join (phils[i], NULL);
    return 0;
}

void posix_set_sched_policy(){
    max_priority = sched_get_priority_max(SCHED_RR);
    printf("max param allowed %d\n", max_priority);
    struct sched_param param = {.sched_priority = max_priority};
    printf("had %d policy, setting %d..\n", sched_getscheduler(0), SCHED_RR);
    sched_setscheduler(0, SCHED_RR, &param);
    printf("set %d policy;\n", sched_getscheduler(0));
}

void lower_priority(int eaten){
    struct sched_param param = {.sched_priority = max_priority - eaten};
    sched_setscheduler(0, SCHED_RR, &param);
}

void *
philosopher (void *num)
{   
    if(policy){
        posix_set_sched_policy();
    }else{
        // actually pthread_setschedparam does same stuff
    }
    int id;
    int left_fork, right_fork, f, eaten = 0;

    id = *((int*)num);
    printf ("Philosopher %d sitting down to dinner.\n", id);
    right_fork = id;
    left_fork = id + 1;

    /* Wrap around the forks. */
    if (left_fork == PHILO)
        left_fork = 0;

    while (f = food_on_table ()) {

        /* Thanks to philosophers #1 who would like to
         * take a nap before picking up the forks, the other
         * philosophers may be able to eat their dishes and
         * not deadlock.
         */
        if (id == 1)
            sleep (sleep_seconds);

        printf ("Philosopher %d: get dish %d.\n", id, f);
        get_forks(id, right_fork, left_fork);

        if(verbosity) printf ("Philosopher %d: eating.\n", id);
        eaten++;
        lower_priority(eaten);
        usleep (DELAY * (FOOD - f + 1));
        down_forks (left_fork, right_fork);
    }
    printf ("Philosopher %d is done eating %d meals.\n", id, eaten);
    return (NULL);
}

int
food_on_table ()
{
    static int food = FOOD;
    int myfood;

    pthread_mutex_lock (&foodlock);
    if (food > 0) {
        food--;
    }
    myfood = food;
    pthread_mutex_unlock (&foodlock);
    return myfood;
}

void
get_fork (int phil,
          int fork,
          char *hand)
{
    pthread_mutex_lock (&forks[fork]);
    if(verbosity) printf ("Philosopher %d: got %s fork %d\n", phil, hand, fork);
}

void get_forks(int id, int right_fork, int left_fork)
{
    if (right_fork < left_fork)
    {
        get_fork (id, right_fork, "right");
        get_fork (id, left_fork, "left ");
    } else
    {
        get_fork (id, left_fork, "left");
        get_fork (id, right_fork, "right");
    }
}

void
down_forks (int right_fork,
            int left_fork)
{
    if (right_fork > left_fork)
    {
        pthread_mutex_unlock (&forks[right_fork]);
        pthread_mutex_unlock (&forks[left_fork]);
    } else
    {
        pthread_mutex_unlock (&forks[left_fork]);
        pthread_mutex_unlock (&forks[right_fork]);
    }
}
