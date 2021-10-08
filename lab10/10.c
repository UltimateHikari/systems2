#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

#define PHILO 5
#define DELAY 30000
#define FOOD 50

/**/#define THINKING 0
#define EATING 1
#define HUNGRY 2/**/
#define LEFT (i+PHILO-1) % PHILO
#define RIGHT (i+1) % PHILO 

pthread_mutex_t forks[PHILO];
pthread_t phils[PHILO];
void *philosopher (void *id);
int food_on_table ();
void get_fork (int, int, char *);
void down_forks (int, int);
pthread_mutex_t foodlock;

/**/pthread_mutex_t forklock;/**/
/**/sem_t s[PHILO];/**/
/**/int state[PHILO];/**/

int
main (int argn,
      char **argv)
{
  int i;

  pthread_mutex_init (&foodlock, NULL);

  /**/pthread_mutex_init (&forklock, NULL);/**/
  for (i = 0; i < PHILO; i++)
    pthread_mutex_init (&forks[i], NULL);
    /**/sem_init (&s[i], 0, 0);/**/
    /**/state[i] = THINKING;/**/
  for (i = 0; i < PHILO; i++)
    pthread_create (&phils[i], NULL, philosopher, (void *)i);
  for (i = 0; i < PHILO; i++)
    pthread_join (phils[i], NULL);

  for (i = 0; i < PHILO; i++)
    sem_destroy (s + i);

  return 0;
}

void *
philosopher (void *num)
{
  int id;
  int left_fork, right_fork, f;

  id = (int)num;
  printf ("Philosopher %d sitting down to dinner.\n", id);
  right_fork = id;
  left_fork = id + 1;
 
  /* Wrap around the forks. */
  if (left_fork == PHILO)
    left_fork = 0;
 
  while (f = food_on_table ()) {
    //no think
    // try take
    pthread_mutex_lock(&forklock);
    printf ("Philosopher %d: try get dish %d.\n", id, f);
    state[id] = HUNGRY;

    test(id);

    pthread_mutex_unlock(&forklock);
    sem_wait(s + id); // if test failed;
    //eat
    printf ("Philosopher %d: eating.\n", id);
    usleep (DELAY * (FOOD - f + 1));

    pthread_mutex_lock(&forklock);
    state[id] = THINKING;
    down_forks (left_fork, right_fork);

    int i = id;
    test(RIGHT);
    test(LEFT);

    pthread_mutex_unlock(&forklock);
  }
  printf ("Philosopher %d is done eating.\n", id);
  return (NULL);
}

void test(int i){
  if(state[i] == HUNGRY && state[LEFT] != EATING && state[RIGHT] != EATING){
    state[i] = EATING;
    sem_post(s + i);
    get_forks(i);
  }
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

void get_forks(int i){
  get_fork (i, i, "right");
  get_fork (i, RIGHT, "left ");
}

void
get_fork (int phil,
          int fork,
          char *hand)
{
  pthread_mutex_lock (&forks[fork]);
  printf ("Philosopher %d: got %s fork %d\n", phil, hand, fork);
}

void
down_forks (int f1,
            int f2)
{
  pthread_mutex_unlock (&forks[f1]);
  pthread_mutex_unlock (&forks[f2]);
}
