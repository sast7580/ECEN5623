#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <math.h>
#include <semaphore.h>
#include <syslog.h>
#include <unistd.h>

// syslog 6 is for informational messages
// syslog 4 is for warning messages
// syslog 3 is for error conditions

typedef struct
{
	int threadIdx;
} threadParams_t;

pthread_t thread[3];
threadParams_t threadParams[3];
pthread_mutex_t key;
pthread_mutex_t timeoutKey;

struct acc
{
	double x;
	double y;
	double z;
};

struct nav
{
	double roll;
	double pitch;
	double yaw;
	struct acc xyzData;
} navigational_state;

double sec, nsec, startsec, startnsec, endsec, endnsec;

// acceleration x axis
double getX()
{
	double x = -40.5;	// left 40.5
	return x;
}

// acceleration y axis
double getY()
{
	double y = 171.7;	// forward 171.7
	return y;
}

// acceleration z axis
double getZ()
{
	double z = 8.8;		// up 8.8
	return z;
}

// roll
double getRoll()
{
	double roll = 13.1;	// right 13.1 degrees
	return roll;
}

// pitch
double getPitch()
{
	double pitch = 30.5;	// tilt up 30.5 degrees
	return pitch;
}

// yaw
double getYaw()
{
	double yaw = 270.0;	// West
	return yaw;
}

// fetch current time fn
void currentTime(double* seconds, double* nseconds)
{
	struct timespec time;

	clock_gettime(CLOCK_REALTIME, &time);
	*seconds = time.tv_sec;
	*nseconds = time.tv_nsec;
}

// updating acceleration and navigation for thread1 and  time for thread2
void* updateData(void *arg)
{
	threadParams_t *threadParams = (threadParams_t *)arg;

	if(threadParams->threadIdx == 1)
	{
		pthread_mutex_lock(&key);

		// get acceleration
		navigational_state.xyzData.x = getX();
		navigational_state.xyzData.y = getY();
		navigational_state.xyzData.z = getZ();

		// get navigation
		navigational_state.roll = getRoll();
		navigational_state.pitch = getPitch();
		navigational_state.yaw = getYaw();
		currentTime(&sec, &nsec);

		pthread_mutex_unlock(&key);
	
		syslog(6, "Acceleration rates = {%.1f, %.1f, %.1f}. Roll = %.1f, pitch = %.1f, and yaw = %.1f updated by thread #%d at %.0lf seconds and %.0lf nanoseconds.", navigational_state.xyzData.x, navigational_state.xyzData.y, navigational_state.xyzData.z, navigational_state.roll, navigational_state.pitch, navigational_state.yaw, threadParams->threadIdx, sec, nsec);
	}

	if(threadParams->threadIdx == 2)
	{
		syslog(6, "Time stamp from last update: %.0lf seconds and %.0lf nanoseconds; read by thread #%d", sec, nsec, threadParams->threadIdx);
	}

}

void timeOut()
{
	// create timer for lock
	struct timespec timer;
	clock_gettime(CLOCK_REALTIME, &timer);
	timer.tv_sec += 10;

	// lock the key to force a timeout
	pthread_mutex_lock(&timeoutKey);
	
	currentTime(&startsec, &startnsec);
//	syslog(6, "startsec = %.0lf sec and %.0lf nsec.", startsec, startnsec);

	int rc = pthread_mutex_timedlock(&timeoutKey, &timer);
	syslog(6, "rc = %d.", rc);

		if(rc != 0)
		{
 			perror("Timeout"); 
		}

	pthread_mutex_unlock(&timeoutKey);
	currentTime(&endsec, &endnsec);
//	syslog(6, "endsec = %.0lf sec and %.0lf nsec.", endsec, endnsec);
	
	int difference = endsec - startsec;
	syslog(6, "No new data available at %d seconds.", timer);
	syslog(6, "Time in lock: %d sec.", difference);

	timeOut();
}

int main()
{
	int rc, i;
	
	// create pthreads
	for(i = 1; i < 3; i++)
	{
		threadParams[i].threadIdx = i;
		rc = pthread_create(&thread[i], (void *)0, updateData, (void *)&(threadParams[i]));
		
		if(rc != 0)
		{
			syslog(3, "Thread[%d] not created. return = %d", i, rc);
			perror(NULL); 
			exit(-1);
		}
	}

	threadParams[3].threadIdx = 3;
	rc = pthread_create(&thread[3], (void *)0, timeOut, (void *)&(threadParams[3]));
		
	if(rc != 0)
	{
		syslog(3, "Thread[%d] not created. return = %d", i, rc);
		perror(NULL); 
		exit(-1);
	}

	// join pthreads
	for(i = 1; i < 3; i++)
	{
       pthread_join(thread[i], NULL);
	}
	pthread_join(thread[3], NULL);
	pthread_mutex_destroy(&timeoutKey);
	pthread_mutex_destroy(&key);
}