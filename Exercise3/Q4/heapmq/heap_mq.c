/************************************************
*    Title: heap_mq                			    *
*    Original Author: Sam Siewert	        	*
*    Modified by: Sarah Stephany 		        *
*    Date: 07/2019			                	*
*    Code version: 2.0				            *
************************************************/

// needed for CPU_ZERO, CPU_SET, CPU_COUNT
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <errno.h> 
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>

// syslog 6 is for informational messages
// syslog 4 is for warning messages
// syslog 3 is for error conditions

#define SNDRCV_MQ "/send_receive_mq"
#define ERROR (-1)
#define NUM_THREADS     2
#define NUM_CORES       1

typedef struct
{
    int threadIdx;
} threadParams_t;

// POSIX thread declarations and scheduling attributes
pthread_t thread[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS];

struct mq_attr mq_attr;
static mqd_t mymq;

// prints the scheduler to ensure operating correctly
void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
        case SCHED_FIFO:
            syslog(6, "Pthread Policy is SCHED_FIFO");
            break;
        case SCHED_OTHER:
            syslog(3, "Pthread Policy is SCHED_OTHER"); exit(-1);
            break;
        case SCHED_RR:
            syslog(3, "Pthread Policy is SCHED_RR"); exit(-1);
            break;
        default:
            syslog(3, "Pthread Policy is UNKNOWN"); exit(-1);
   }
}

/* receives pointer to heap, reads it, and deallocate heap memory */
void *receiver(void *arg)
{
    syslog(6, "RECEIVER:");

  char buffer[sizeof(void *)+sizeof(int)];
  int prio;
  void *buffptr;
  int nbytes;
  int count = 0;
  int id;
 
  while(1) {

    /* read oldest, highest priority msg from the message queue */

    syslog(6, "Reading %ld bytes", sizeof(void *));
  
    if((nbytes = mq_receive(mymq, buffer, (size_t)(sizeof(void *)+sizeof(int)), &prio)) == ERROR)
    {
      perror("mq_receive");
      exit(-1);
    }
    else
    {
        memcpy(&buffptr, buffer, sizeof(void *));
        memcpy((void *)&id, &(buffer[sizeof(void *)]), sizeof(int));
        syslog(6, "receive: ptr msg 0x%X received with priority = %d, length = %d, id = %d", buffptr, prio, nbytes, id);

        syslog(6, "contents of ptr = %s", (char *)buffptr);
            
        free(buffptr);

        syslog(6, "heap space memory freed");
    }
  }
}

static char imagebuff[4096];

void *sender(void *arg)
{
    syslog(6, "SENDER:");

  char buffer[sizeof(void *)+sizeof(int)];
  int prio;
  void *buffptr;
  int nbytes;
  int id = 999;

  while(1) {

    /* send malloc'd message with priority=30 */
    buffptr = (void *)malloc(sizeof(imagebuff));

    strcpy(buffptr, imagebuff);
    syslog(6, "Message to send = %s", (char *)buffptr);

    syslog(6, "Sending %ld bytes", sizeof(buffptr));

    memcpy(buffer, &buffptr, sizeof(void *));
    memcpy(&(buffer[sizeof(void *)]), (void *)&id, sizeof(int));

    if((nbytes = mq_send(mymq, buffer, (size_t)(sizeof(void *)+sizeof(int)), 30)) == ERROR)
    {
      perror("mq_send");
      exit(-1);
    }
    else
    {
      syslog(6, "send: message ptr 0x%X successfully sent", buffptr);
    }

    usleep(3000);
  } 
}

static int sid, rid;

int main()
{
    int i, j, rc, scope, rt_max_prio, rt_min_prio;
    char pixel = 'A';

    struct sched_param rt_param[NUM_THREADS];
    struct sched_param main_param;

    pthread_t thread[NUM_THREADS];
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
    syslog(6, "%d core set", CPU_COUNT(&allcpuset));

    mainpid=getpid();

    // set priority and parameters
    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);
    rc = sched_getparam(mainpid, &main_param);
    main_param.sched_priority = rt_max_prio;
    rc = sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    syslog(6, "rt_max_prio=%d", rt_max_prio);
    syslog(6, "rt_min_prio=%d", rt_min_prio);

    if(rc < 0)
    {
        perror("main_param");
        exit(-1);
    }

    // print scheduler
    print_scheduler();

    // set scope
    pthread_attr_getscope(&main_attr, &scope);

    if(scope == PTHREAD_SCOPE_SYSTEM)
    {
        syslog(6, "PTHREAD SCOPE SYSTEM");
    }
    else if (scope == PTHREAD_SCOPE_PROCESS)
    {
        syslog(6, "PTHREAD SCOPE PROCESS");
    }
    else
    {
        syslog(6, "PTHREAD SCOPE UNKNOWN");
    }

    // test buffer
    for(i=0;i<4096;i+=64) 
    {
        pixel = 'A';

        for(j=i;j<i+64;j++) 
        {
            imagebuff[j] = (char)pixel++;
        }

        imagebuff[j-1] = '\n';
    }

    imagebuff[4095] = '\0';
    imagebuff[63] = '\0';

    syslog(6, "buffer = %s", imagebuff);

    // setup common message queue attributes
    mq_attr.mq_maxmsg = 100;
    mq_attr.mq_msgsize = sizeof(void *)+sizeof(int);
    mq_attr.mq_flags = 0;

    // create the message queue
    mymq = mq_open(SNDRCV_MQ, O_CREAT|O_RDWR, 0664, &mq_attr);

    mq_unlink(SNDRCV_MQ);

    if(mymq == (mqd_t)ERROR)
    {
        perror("mq_open");
        exit(-1);
    }

    // initialize threads
    for(i=0; i < NUM_THREADS; i++)
    {
        //CPU_ZERO(&threadcpu);
        //CPU_SET(i, &threadcpu);

        rc = pthread_attr_init(&rt_sched_attr[i]);
        rc = pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
        rc = pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
        rc = pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);

        rt_param[i].sched_priority=rt_max_prio-i;
        pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

        threadParams[i].threadIdx=i;
    }

    // receiver thread runs at a higher priority than the sender
    threadParams[0].threadIdx = 0;
    rc = pthread_create(&thread[0], &rt_sched_attr[0], receiver, NULL);
		
	if(rc != 0)
	{
		syslog(3, "Receiver thread not created. return = %d", rc);
		perror(NULL); 
		exit(-1);
	}
    else
        syslog(6, "Receiver thread created");

    // sender thread
    threadParams[1].threadIdx = 1;
    rc = pthread_create(&thread[1], &rt_sched_attr[1], sender, NULL);
		
	if(rc != 0)
	{
		syslog(3, "Sender thread not created. return = %d", rc);
		perror(NULL); 
		exit(-1);
	}
    else
        syslog(6, "Sender thread created");

    pthread_join(thread[1], NULL);
    pthread_join(thread[2], NULL);

    mq_close(mymq);

    syslog(6, "SHUTDOWN");
}