/************************************************
*    Title: Pthreads                			*
*    Original Author: Sam Siewert	        	*
*    Modified by: Sarah Stephany 		        *
*    Date: 06/2019			                	*
*    Code version: 2.0				            *
************************************************/

// needed for CPU_ZERO, CPU_SET, CPU_COUNT
#define _GNU_SOURCE

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <syslog.h>
#include <time.h>
#include <semaphore.h>

#define NUM_THREADS     3
#define NUM_CORES       1
#define U_TO_MILLI_10   (10*1000)
#define U_TO_MILLI_20   (20*1000)

typedef struct
{
    int threadIdx;
} threadParams_t;

// global start time
double start_time;
int testabort = 0;

// semaphores for 10 msec and 20 msec
sem_t semT10;
sem_t semT20;

// POSIX thread declarations and scheduling attributes
pthread_t threads[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS];

int Fibonacci_Test(sequence, iteration)
{
    int temp = 0, first = 0, second = 1, increment = 1;

    for(int f = 0; f < iteration; f++)
    {
        temp = first + second;
        while(increment < sequence)
        {
            first = second;
            second = temp;
            temp = first + second;
            increment++;
        }
    }
}

void *Fib10(void *threadp)
{
    int i = 0, count = 0, core, cycles_needed;
    double test_time = 0.0, end_time = 0.0, run_time = 0.0, ms;
    
    struct timespec start, finish;

    threadParams_t *threadParams = (threadParams_t *)threadp;

    // test how long for fibonacci
    Fibonacci_Test(10, 100);    
    clock_gettime(CLOCK_MONOTONIC, &start);
    ms = ((start.tv_sec)*1000.0) + ((start.tv_nsec) / 1.0e6);
    test_time = ms; 
    syslog(LOG_INFO, "test start time for fibonacci");
    Fibonacci_Test(10, 100);
    clock_gettime(CLOCK_MONOTONIC, &finish);
    ms = ((finish.tv_sec)*1000.0) + ((finish.tv_nsec) / 1.0e6);
    end_time = ms; 
    syslog(LOG_INFO, "test end time for fibonacci");
    run_time = (end_time - test_time);
    printf("\nF10 test runtime -> %lf - %lf = %lf msec\n", end_time, test_time, run_time);

    // determine if more fibonacci cycles are needed
    cycles_needed = (int)(10/run_time);
    printf("%d cycles needed for fib10\n", cycles_needed);

    // run thread
    while(!testabort)
    {
        sem_wait(&semT10);

        if(testabort)
        {
            break;
        }
        
        core = sched_getcpu();
        i++;
        clock_gettime(CLOCK_MONOTONIC, &finish);
        ms = ((finish.tv_sec)*1000.0) + ((finish.tv_nsec) / 1.0e6);
        end_time = ms;    
        syslog(LOG_INFO, "test start time for Fib10 loop");    
        printf("F10 start time #%d = %lf.  Core %d used.\n", i, (end_time - start_time), core);
        
        do
        {
            Fibonacci_Test(10, 100);
            count++;
        }
        while(count < cycles_needed);

        clock_gettime(CLOCK_MONOTONIC, &finish);
        ms = ((finish.tv_sec)*1000.0) + ((finish.tv_nsec) / 1.0e6);
        end_time = ms;       
        syslog(LOG_INFO, "test end time for Fib10 loop");                 
        printf("F10 completion time #%d = %lf.  Core %d used.\n", i, (end_time - start_time), core);

        count = 0;
        pthread_join(threads[1], NULL);
    }
}

void *Fib20(void *threadp)
{
    int i = 0, count = 0, core, cycles_needed;
    double test_time = 0.0, end_time = 0.0, run_time = 0.0, ms;
    
    struct timespec start, finish;

    threadParams_t *threadParams = (threadParams_t *)threadp;

    // test how long for fibonacci
    clock_gettime(CLOCK_MONOTONIC, &start);
    ms = ((start.tv_sec)*1000.0) + ((start.tv_nsec) / 1.0e6);
    test_time = ms; 
    syslog(LOG_INFO, "test start time for Fib20 loop");
    Fibonacci_Test(10, 100);
    clock_gettime(CLOCK_MONOTONIC, &finish);
    ms = ((finish.tv_sec)*1000.0) + ((finish.tv_nsec) / 1.0e6);
    end_time = ms; 
    syslog(LOG_INFO, "test end time for Fib20 loop");
    run_time = (end_time - test_time);
    printf("F20 test runtime -> %lf - %lf = %lf msec\n", end_time, test_time, run_time);

    // determine if more fibonacci cycles are needed
    cycles_needed = (int)(20/run_time);
    printf("%d cycles needed for fib20\n", cycles_needed);

    // run thread
    while(!testabort)
    {
        sem_wait(&semT20);

        if(testabort)
        {
            break;
        }

        core = sched_getcpu();
        i++;
        clock_gettime(CLOCK_MONOTONIC, &finish);
        ms = ((finish.tv_sec)*1000.0) + ((finish.tv_nsec) / 1.0e6);
        end_time = ms; 
        syslog(LOG_INFO, "F20 start time in run thread");              
        printf("F20 start time #%d = %lf.  Core %d used.\n", i, (end_time - start_time), core);

        do
        {
            Fibonacci_Test(10, 100);
            count++;
        }
        while(count < cycles_needed);

        clock_gettime(CLOCK_MONOTONIC, &finish);
        ms = ((finish.tv_sec)*1000.0) + ((finish.tv_nsec) / 1.0e6);
        end_time = ms;  
        syslog(LOG_INFO, "F20 completion time in run thread");          
        printf("F20 completion time #%d = %lf.  Core %d used.\n", i, (end_time - start_time), core);

        count = 0;
        pthread_join(threads[2], NULL);
    }
}

void *sequencer(void *threadp)
{
    int i = 0;
    double previous_time = 0.0, end_time = 0.0, ms;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    struct timespec start, finish;

    clock_gettime(CLOCK_MONOTONIC, &start);
    ms = ((start.tv_sec)*1000.0) + ((start.tv_nsec) / 1.0e6);
    start_time = ms;    
    syslog(LOG_INFO, "system start time");

    do
    {
        printf("\nStart sequencer: T1=20, C1=10, T2=50, C2=20, U=0.9, LCM=100\n");
        
        // T1 before 20 msec
        sem_post(&semT10);
        usleep(U_TO_MILLI_20);
        clock_gettime(CLOCK_MONOTONIC, &finish);
        ms = ((finish.tv_sec)*1000.0) + ((finish.tv_nsec) / 1.0e6);
        end_time = ms; 
        syslog(LOG_INFO, "total time calculation F10 #1"); 
        printf("t = %lf\n", (end_time - start_time));
        printf("total t = %lf\n\n", (end_time - start_time));      
        previous_time = end_time - start_time;   

        // T1 before 40 msec
        sem_post(&semT10);
        usleep(U_TO_MILLI_20);       
        clock_gettime(CLOCK_MONOTONIC, &finish);
        ms = ((finish.tv_sec)*1000.0) + ((finish.tv_nsec) / 1.0e6);
        end_time = ms; 
        syslog(LOG_INFO, "total time calculation F20 #1"); 
        printf("t = %lf\n", ((end_time - start_time)-previous_time));
        printf("total t = %lf\n\n", (end_time - start_time));
        previous_time = end_time - start_time;
       
        // T2 before 50 msec
        sem_post(&semT20);       
        usleep(U_TO_MILLI_10);
        clock_gettime(CLOCK_MONOTONIC, &finish);
        ms = ((finish.tv_sec)*1000.0) + ((finish.tv_nsec) / 1.0e6);
        end_time = ms; 
        syslog(LOG_INFO, "total time calculation Fib10 #2");
        printf("t = %lf\n", ((end_time - start_time) - previous_time));
        printf("total t = %lf\n\n", (end_time - start_time));
        previous_time = end_time - start_time;
    
        // T1 before 60 msec
        sem_post(&semT10);
        usleep(U_TO_MILLI_10);        
        clock_gettime(CLOCK_MONOTONIC, &finish);
        ms = ((finish.tv_sec)*1000.0) + ((finish.tv_nsec) / 1.0e6);
        end_time = ms; 
        syslog(LOG_INFO, "total time calculation Fib20 #2");        
        printf("t = %lf\n", ((end_time - start_time) - previous_time));
        printf("total t = %lf\n\n", (end_time - start_time));
        previous_time = end_time - start_time;

        // T1 before 80 msec
        sem_post(&semT10);
        usleep(U_TO_MILLI_20);
        clock_gettime(CLOCK_MONOTONIC, &finish);
        ms = ((finish.tv_sec)*1000.0) + ((finish.tv_nsec) / 1.0e6);
        end_time = ms; 
        syslog(LOG_INFO, "total time calculation Fib10 #3");
        printf("t = %lf\n", ((end_time - start_time) - previous_time));
        printf("total t = %lf\n\n", (end_time - start_time));
        previous_time = end_time - start_time;

        i++;
    }
    while(i < NUM_THREADS);

    // end
    testabort = 1;

    sem_post(&semT10);
    sem_post(&semT20);
}

void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
        case SCHED_FIFO:
            printf("Pthread Policy is SCHED_FIFO\n");
            break;
        case SCHED_OTHER:
            printf("Pthread Policy is SCHED_OTHER\n"); exit(-1);
            break;
        case SCHED_RR:
            printf("Pthread Policy is SCHED_RR\n"); exit(-1);
            break;
        default:
            printf("Pthread Policy is UNKNOWN\n"); exit(-1);
   }

}

int main (int argc, char *argv[])
{
   int rc, scope, i, rt_max_prio, rt_min_prio;
   struct sched_param rt_param[NUM_THREADS];
   struct sched_param main_param;

   pthread_t threads[NUM_THREADS];
   threadParams_t threadParams[NUM_THREADS];
   pthread_attr_t rt_sched_attr[NUM_THREADS];
   pthread_attr_t main_attr;
   pid_t mainpid;

   cpu_set_t allcpuset;
   cpu_set_t threadcpu;

   // set processor core
   CPU_ZERO(&allcpuset);

   for(i = 0; i < NUM_CORES; i++)
   {
       CPU_SET(i, &allcpuset);
   }
   printf("%d core set\n", CPU_COUNT(&allcpuset));

   // initialize semaphores
    if (sem_init (&semT10, 0, 0)) 
    { 
        printf ("Failed to initialize semaphore T10\n"); 
        exit (-1); 
    }

    if (sem_init (&semT20, 0, 0)) 
    { 
        printf ("Failed to initialize semaphore T20\n"); 
        exit (-1); 
    }
      
    mainpid=getpid();

    // set priority and parameters
    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);
    rc = sched_getparam(mainpid, &main_param);
    main_param.sched_priority = rt_max_prio;
    rc = sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    printf("rt_max_prio=%d\n", rt_max_prio);
    printf("rt_min_prio=%d\n", rt_min_prio);

    if(rc < 0)
    {
        perror("main_param");
    }

    // start scheduler
    print_scheduler();

    // set scope
    pthread_attr_getscope(&main_attr, &scope);

    if(scope == PTHREAD_SCOPE_SYSTEM)
    {
        printf("PTHREAD SCOPE SYSTEM\n");
    }
    else if (scope == PTHREAD_SCOPE_PROCESS)
    {
        printf("PTHREAD SCOPE PROCESS\n");
    }
    else
    {
        printf("PTHREAD SCOPE UNKNOWN\n");
    }

    // initialize threads
    for(i=0; i < NUM_THREADS; i++)
    {
        CPU_ZERO(&threadcpu);
        CPU_SET(i, &threadcpu);

        rc = pthread_attr_init(&rt_sched_attr[i]);
        rc = pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
        rc = pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
        rc = pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);

        rt_param[i].sched_priority=rt_max_prio-i;
        pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

        threadParams[i].threadIdx=i;
    }
   
    // create thread T10
    rc = pthread_create(&threads[1], &rt_sched_attr[1], Fib10, (void *)&(threadParams[1]));
    // create thread T20
    rc = pthread_create(&threads[2], &rt_sched_attr[2], Fib20, (void *)&(threadParams[2]));

    // Wait for release by sequencer
    usleep(300000);
 
    // create sequencer thread (highest priority)
    rc = pthread_create(&threads[0], &rt_sched_attr[0], sequencer, (void *)&(threadParams[0]));

    for(i=0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    printf("\nTEST COMPLETE\n");
}
