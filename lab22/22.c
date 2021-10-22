#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "verify.h"

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
pthread_mutex_t foodlock;
pthread_mutex_t monlock;
pthread_cond_t monitor;
int food = FOOD;

void *philosopher (void *id);
int food_on_table ();
int try_get_fork (int, int, char *);
void drop_food ();
void drop_fork (int, int, char *);
void drop_forks (int, int, int);


void destroy_foodlock(){
  verify(pthread_mutex_destroy(&foodlock), "destroy foodlock", NO_CLEANUP);
}

void destroy_monitor_m(){
  destroy_foodlock();
  verify(pthread_mutex_destroy(&monlock), "destroy monlock", NO_CLEANUP);
}

void destroy_monitor_c(){
  destroy_monitor_m();
  verify(pthread_cond_destroy(&monitor), "destroy cond", NO_CLEANUP);
}

void destroy_all(){
  destroy_monitor_c();
  for(int i = 0; i < PHILO; i++){
    verify(pthread_mutex_destroy(forks + i), "destroy fork", NO_CLEANUP);
  }
}

int
main ()
{
  int i;

  verify(pthread_mutex_init (&foodlock, NULL), "foodlock", NO_CLEANUP);
  verify(pthread_mutex_init (&monlock, NULL), "monitor", destroy_foodlock);
  verify(pthread_cond_init (&monitor, NULL), "monitor", destroy_monitor_m);

  for (i = 0; i < PHILO; i++)
    verify(pthread_mutex_init (&forks[i], NULL), "fork", destroy_monitor_c);
  for (i = 0; i < PHILO; i++)
    verify(pthread_create (&phils[i], NULL, philosopher, (void *)i), "create", destroy_all);
  for (i = 0; i < PHILO; i++)
    verify(pthread_join (phils[i], NULL), "join", destroy_all);

  destroy_all();

  return 0;
}

void *
philosopher (void *num)
{
  int id;
  int left_fork, right_fork, f;
  int eaten = 0;

  id = (int)num;
  printf ("Philosopher %d sitting down to dinner.\n", id);
  right_fork = id;
  left_fork = id + 1;
 
  /* Wrap around the forks. */
  if (left_fork == PHILO)
    left_fork = 0;
 
  while ((f = food_on_table ())) {
    //no think
    /**
     * try take
     * if spurious wakeup / broadcast wakeup, we're rushing for mutex+mon+left fork again
     * bc our condition is outcome of trylock, not global var
     */ 
    verify(pthread_mutex_lock(&monlock), "monlock", destroy_all);
    printf ("Philosopher %d: try get dish %d.\n", id, f);
    
    if(!try_get_fork(id, left_fork, "left")){

      drop_food();
      verify(pthread_cond_wait(&monitor, &monlock), "sleeping", destroy_all);
      verify(pthread_mutex_unlock(&monlock), "monunlock", destroy_all);

    } else {

      printf ("Philosopher %d: got     left fork %d\n", id, left_fork);

      if (!try_get_fork(id, right_fork, "right")){
        drop_food();
        drop_fork(id, left_fork, "left");

        verify(pthread_cond_wait(&monitor, &monlock), "sleeping", destroy_all);
        verify(pthread_mutex_unlock(&monlock), "monunlock", destroy_all);

      } else {

        printf ("Philosopher %d: got     right fork %d\n\n", id, right_fork);
        // have both forks now, can eat
        verify(pthread_mutex_unlock(&monlock), "monunlock", destroy_all);
        
        //eat
        eaten++;
        printf ("Philosopher %d: eating  %d.\n", id, f);
        usleep (DELAY * (FOOD - f + 1));

        // need that mutex for SIG_FATWAKEUP
        verify(pthread_mutex_lock(&monlock), "monlock", destroy_all);
          drop_forks (id, left_fork, right_fork);
          //do not care if signal or broadcast
          verify(pthread_cond_broadcast(&monitor), "SIG_FATWAKEUP", destroy_all);
        verify(pthread_mutex_unlock(&monlock), "monunlock", destroy_all);

      }

    }
    
  }
  printf ("Philosopher %d is done eating %d meals.\n", id, eaten);
  return (NULL);
}

int
food_on_table ()
{
  int myfood;

  pthread_mutex_lock (&foodlock);
  if (food > 0) {
    food--;
  }
  myfood = food;
  pthread_mutex_unlock (&foodlock);
  return myfood;
}

void drop_food (){
  pthread_mutex_lock (&foodlock);
  food++;
  pthread_mutex_unlock (&foodlock);
}

int
try_get_fork (int phil,
          int fork,
          char *hand)
{
  printf ("Philosopher %d: trying  %s fork %d\n", phil, hand, fork);
  return (pthread_mutex_trylock (&forks[fork]) == 0);
}

void
drop_fork (int phil,
          int fork,
          char *hand)
{
  verify(pthread_mutex_unlock (&forks[fork]), "forkunlock", destroy_all);
  printf ("Philosopher %d: dropped %s fork %d\n", phil, hand, fork);
}

void
drop_forks (int id,
            int f1,
            int f2)
{
  drop_fork(id, f1, "left");
  drop_fork(id, f2, "right");
}
