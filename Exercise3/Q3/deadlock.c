/************************************************
*    Title: deadlock                  			    *
*    Original Author: Sam Siewert   	        	*
*    Modified by: Sarah Stephany 		            *
*    Date: 07/2019			                	      *
*    Code version: 2.0				                  *
************************************************/

#include <pthread.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>

// syslog 6 is for informational messages
// syslog 4 is for warning messages
// syslog 3 is for error conditions

#define NUM_THREADS 2
#define THREAD_1 1
#define THREAD_2 2

void backOff()
{
  int waitTime = 1000000, maxWait = 3000000;

  while (waitTime < maxWait)
  {
    usleep(waitTime);
    waitTime += rand();
    syslog(6, "Backoff alg wait time = %d", waitTime);
  }

  return;
}

typedef struct
{
  int threadIdx;
} threadParams_t;


pthread_t threads[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS];

struct sched_param nrt_param;

pthread_mutex_t rsrcA, rsrcB;

volatile int rsrcACnt=0, rsrcBCnt=0, noWait=0;


void *grabRsrcs(void *threadp)
{
   threadParams_t *threadParams = (threadParams_t *)threadp;
   int threadIdx = threadParams->threadIdx;

   if(threadIdx == THREAD_1)
   {
      syslog(6, "THREAD 1 grabbing resources");
      
      // mutex locked, wait for unlock
      if (pthread_mutex_trylock(&rsrcA) != 0)
      {
        syslog(4, "A blocked, using backoff alg");
        backOff();
      }

      rsrcACnt++;
      syslog(6, "THREAD 1 got A, trying for B");

      // mutex locked, wait for unlock
      if (pthread_mutex_trylock(&rsrcB) != 0)
      {
        syslog(4, "B blocked, using backoff alg");
        backOff();
      }

      rsrcBCnt++;
      syslog(6, "THREAD 1 got A and B");
      pthread_mutex_unlock(&rsrcB);
      pthread_mutex_unlock(&rsrcA);
      syslog(6, "THREAD 1 done");
   }
   else
   {
      syslog(6, "THREAD 2 grabbing resources");
      
      // mutex locked, wait for unlock     
      if (pthread_mutex_trylock(&rsrcB) != 0)
      {
        syslog(4, "B blocked, using backoff alg");
        backOff();
      }

      rsrcBCnt++;
      syslog(6, "THREAD 2 got B, trying for A");
      
      // mutex locked, wait for unlock
      if (pthread_mutex_trylock(&rsrcA) != 0)
      {
        syslog(4, "A blocked, using backoff alg");
        backOff();
      }

      rsrcACnt++;
      syslog(6, "THREAD 2 got B and A");
      pthread_mutex_unlock(&rsrcA);
      pthread_mutex_unlock(&rsrcB);
      syslog(6, "THREAD 2 done");
   }

   pthread_exit(NULL);
}

int main (int argc, char *argv[])
{
   int rc, safe=0;

   rsrcACnt=0, rsrcBCnt=0, noWait=0;

   if(argc < 2)
   {
     syslog(6, "Will set up unsafe deadlock scenario");
   }
   else if(argc == 2)
   {
     if(strncmp("safe", argv[1], 4) == 0)
       safe=1;
     else if(strncmp("race", argv[1], 4) == 0)
       noWait=1;
     else
       syslog(6, "Will set up unsafe deadlock scenario");
   }
   else
   {
     syslog(6, "Usage: deadlock [safe|race|unsafe]");
   }

   // Set default protocol for mutex
   pthread_mutex_init(&rsrcA, NULL);
   pthread_mutex_init(&rsrcB, NULL);

   syslog(6, "Creating thread %d", THREAD_1);
   threadParams[THREAD_1].threadIdx=THREAD_1;
   rc = pthread_create(&threads[0], NULL, grabRsrcs, (void *)&threadParams[THREAD_1]);
   if (rc) {syslog(3, "ERROR; pthread_create() rc is %d", rc); perror(NULL); exit(-1);}
   syslog(6, "Thread 1 spawned");

   if(safe) // Make sure Thread 1 finishes with both resources first
   {
     if(pthread_join(threads[0], NULL) == 0)
       syslog(6, "Thread 1: %x done", (unsigned int)threads[0]);
     else
       perror("Thread 1");
   }

   syslog(6, "Creating thread %d", THREAD_2);
   threadParams[THREAD_2].threadIdx=THREAD_2;
   rc = pthread_create(&threads[1], NULL, grabRsrcs, (void *)&threadParams[THREAD_2]);
   if (rc) {syslog(3, "ERROR; pthread_create() rc is %d", rc); perror(NULL); exit(-1);}
   syslog(6, "Thread 2 spawned");

   syslog(6, "rsrcACnt=%d, rsrcBCnt=%d", rsrcACnt, rsrcBCnt);
   syslog(6, "Will try to join CS threads unless they deadlock");

   if(!safe)
   {
     if(pthread_join(threads[0], NULL) == 0)
       syslog(6, "Thread 1: %x done", (unsigned int)threads[0]);
     else
       perror("Thread 1");
   }

   if(pthread_join(threads[1], NULL) == 0)
     syslog(6, "Thread 2: %x done", (unsigned int)threads[1]);
   else
     perror("Thread 2");

   if(pthread_mutex_destroy(&rsrcA) != 0)
     perror("mutex A destroy");

   if(pthread_mutex_destroy(&rsrcB) != 0)
     perror("mutex B destroy");

   syslog(6, "All done");

   exit(0);
}