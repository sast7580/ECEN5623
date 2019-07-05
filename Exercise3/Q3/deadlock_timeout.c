/************************************************
*    Title: deadlock_timeout           			*
*    Original Author: Sam Siewert   	        *
*    Modified by: Sarah Stephany 		        *
*    Date: 07/2019			                    *
*    Code version: 2.0				            *
************************************************/

#include <pthread.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>

// syslog 6 is for informational messages
// syslog 4 is for warning messages
// syslog 3 is for error conditions

#define NUM_THREADS 2
#define THREAD_1 1
#define THREAD_2 2

typedef struct
{
    int threadIdx;
} threadParams_t;


threadParams_t threadParams[NUM_THREADS];
pthread_t threads[NUM_THREADS];
struct sched_param nrt_param;

pthread_mutex_t rsrcA, rsrcB;

volatile int rsrcACnt=0, rsrcBCnt=0, noWait=0;


void *grabRsrcs(void *threadp)
{
   struct timespec timeNow;
   struct timespec rsrcA_timeout;
   struct timespec rsrcB_timeout;
   int rc;
   threadParams_t *threadParams = (threadParams_t *)threadp;
   int threadIdx = threadParams->threadIdx;

   if(threadIdx == THREAD_1) syslog(6, "Thread 1 started");
   else if(threadIdx == THREAD_2) syslog(6, "Thread 2 started");
   else syslog(4, "Unknown thread started");

   clock_gettime(CLOCK_REALTIME, &timeNow);

   rsrcA_timeout.tv_sec = timeNow.tv_sec + 2;
   rsrcA_timeout.tv_nsec = timeNow.tv_nsec;
   rsrcB_timeout.tv_sec = timeNow.tv_sec + 3;
   rsrcB_timeout.tv_nsec = timeNow.tv_nsec;


   if(threadIdx == THREAD_1)
   {
     syslog(6, "THREAD 1 grabbing resource A @ %d sec and %d nsec", (int)timeNow.tv_sec, (int)timeNow.tv_nsec);
     //if((rc=pthread_mutex_timedlock(&rsrcA, &rsrcA_timeout)) != 0)
     if((rc = pthread_mutex_lock(&rsrcA)) != 0)
     {
         syslog(3, "Thread 1 ERROR");
         pthread_exit(NULL);
     }
     else
     {
         syslog(6, "Thread 1 GOT A");
         rsrcACnt++;
         syslog(6, "rsrcACnt=%d, rsrcBCnt=%d", rsrcACnt, rsrcBCnt);
     }

     // if unsafe test, immediately try to acquire rsrcB
     if(!noWait) usleep(1000000);

     clock_gettime(CLOCK_REALTIME, &timeNow);
     rsrcB_timeout.tv_sec = timeNow.tv_sec + 3;
     rsrcB_timeout.tv_nsec = timeNow.tv_nsec;

     syslog(6, "THREAD 1 got A, trying for B @ %d sec and %d nsec", (int)timeNow.tv_sec, (int)timeNow.tv_nsec);

     rc=pthread_mutex_timedlock(&rsrcB, &rsrcB_timeout);
     //rc=pthread_mutex_lock(&rsrcB);
     if(rc == 0)
     {
         clock_gettime(CLOCK_REALTIME, &timeNow);
         syslog(6, "Thread 1 GOT B @ %d sec and %d nsec with rc=%d", (int)timeNow.tv_sec, (int)timeNow.tv_nsec, rc);
         rsrcBCnt++;
         syslog(6, "rsrcACnt=%d, rsrcBCnt=%d", rsrcACnt, rsrcBCnt);
     }
     else if(rc == ETIMEDOUT)
     {
         syslog(3, "Thread 1 TIMEOUT ERROR");
         rsrcACnt--;
         pthread_mutex_unlock(&rsrcA);
         //pthread_exit(NULL);
     }
     else
     {
         syslog(3, "Thread 1 ERROR");
         rsrcACnt--;
         pthread_mutex_unlock(&rsrcA);
         pthread_exit(NULL);
     }

     syslog(6, "THREAD 1 got A and B");
     rsrcBCnt--;
     pthread_mutex_unlock(&rsrcB);
     rsrcACnt--;
     pthread_mutex_unlock(&rsrcA);
     syslog(6, "THREAD 1 done");
   }

   else
   {
     syslog(6, "THREAD 2 grabbing resource B @ %d sec and %d nsec", (int)timeNow.tv_sec, (int)timeNow.tv_nsec);
     //if((rc=pthread_mutex_timedlock(&rsrcB, &rsrcB_timeout)) != 0)
     if((rc=pthread_mutex_lock(&rsrcB)) != 0)
     {
         syslog(3, "Thread 2 ERROR");
         pthread_exit(NULL);
     }
     else
     {
         syslog(6, "Thread 2 GOT B");
         rsrcBCnt++;
         syslog(6, "rsrcACnt=%d, rsrcBCnt=%d", rsrcACnt, rsrcBCnt);
     }

     // if unsafe test, immediately try to acquire rsrcB
     if(!noWait) usleep(1000000);

     clock_gettime(CLOCK_REALTIME, &timeNow);
     rsrcA_timeout.tv_sec = timeNow.tv_sec + 2;
     rsrcA_timeout.tv_nsec = timeNow.tv_nsec;

     syslog(6, "THREAD 2 got B, trying for A @ %d sec and %d nsec", (int)timeNow.tv_sec, (int)timeNow.tv_nsec);
     rc=pthread_mutex_timedlock(&rsrcA, &rsrcA_timeout);

     if(rc == 0)
     {
         clock_gettime(CLOCK_REALTIME, &timeNow);
         syslog(6, "Thread 2 GOT A @ %d sec and %d nsec with rc=%d", (int)timeNow.tv_sec, (int)timeNow.tv_nsec, rc);
         rsrcACnt++;
         syslog(6, "rsrcACnt=%d, rsrcBCnt=%d", rsrcACnt, rsrcBCnt);
     }
     else if(rc == ETIMEDOUT)
     {
         syslog(3, "Thread 2 TIMEOUT ERROR");
         rsrcBCnt--;
         pthread_mutex_unlock(&rsrcB);
         //pthread_exit(NULL);
     }
     else
     {
         syslog(3, "Thread 2 ERROR");
         rsrcBCnt--;
         pthread_mutex_unlock(&rsrcB);
         pthread_exit(NULL);
     }

     syslog(6, "THREAD 2 got B and A");
     rsrcACnt--;
     pthread_mutex_unlock(&rsrcA);
     rsrcBCnt--;
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

   // Set default protocol for mutex which is unlocked to start
   pthread_mutex_init(&rsrcA, NULL);
   pthread_mutex_init(&rsrcB, NULL);

   syslog(6, "Creating thread %d", THREAD_1);
   threadParams[THREAD_1].threadIdx=THREAD_1;
   rc = pthread_create(&threads[0], NULL, grabRsrcs, (void *)&threadParams[THREAD_1]);
   if (rc) {syslog(3, "ERROR; pthread_create() rc is %d", rc); perror(NULL); exit(-1);}

   if(safe) // Make sure Thread 1 finishes with both resources first
   {
     if(pthread_join(threads[0], NULL) == 0)
       syslog(6, "Thread 1 joined to main");
     else
       perror("Thread 1");
   }

   syslog(6, "Creating thread %d", THREAD_2);
   threadParams[THREAD_2].threadIdx=THREAD_2;
   rc = pthread_create(&threads[1], NULL, grabRsrcs, (void *)&threadParams[THREAD_2]);
   if (rc) {syslog(3, "ERROR; pthread_create() rc is %d", rc); perror(NULL); exit(-1);}

   syslog(6, "Will try to join both CS threads unless they deadlock");

   if(!safe)
   {
     if(pthread_join(threads[0], NULL) == 0)
       syslog(6, "Thread 1 joined to main");
     else
       perror("Thread 1");
   }

   if(pthread_join(threads[1], NULL) == 0)
     syslog(6, "Thread 2 joined to main");
   else
     perror("Thread 2");

   if(pthread_mutex_destroy(&rsrcA) != 0)
     perror("mutex A destroy");

   if(pthread_mutex_destroy(&rsrcB) != 0)
     perror("mutex B destroy");

   syslog(6, "All done");

   exit(0);
}
