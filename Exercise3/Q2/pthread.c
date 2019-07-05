#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <math.h>
#include <semaphore.h>
#include <syslog.h>

// syslog 6 is for informational messages
// syslog 4 is for warning messages
// syslog 3 is for error conditions

typedef struct
{
	int threadIdx;
} threadParams_t;

pthread_t thread[2];
threadParams_t threadParams[2];
pthread_mutex_t key;

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

double sec, nsec;

double getX()
{
	double x = -40.5;	// left 40.5
	return x;
}

double getY()
{
	double y = 171.7;	// forward 171.7
	return y;
}

double getZ()
{
	double z = 8.8;		// up 8.8
	return z;
}

double getRoll()
{
	double roll = 13.1;	// right 13.1 degrees
	return roll;
}

double getPitch()
{
	double pitch = 30.5;	// tilt up 30.5 degrees
	return pitch;
}

double getYaw()
{
	double yaw = 270.0;	// West
	return yaw;
}

void currentTime(double* seconds, double* nseconds)
{
	struct timespec time;

	clock_gettime(CLOCK_REALTIME, &time);
	*seconds = time.tv_sec;
	*nseconds = time.tv_nsec;
}

void* updateAcceleration(void *arg)
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

int main()
{
	int rc, i;
	
	// create pthreads
	for(i = 1; i < 3; i++)
	{
		threadParams[i].threadIdx = i;
		rc = pthread_create(&thread[i], (void *)0, updateAcceleration, (void *)&(threadParams[i]));
		
		if(rc != 0)
		{
			syslog(3, "Thread[%d] not created. return = %d", i, rc);
			perror(NULL); 
			exit(-1);
		}
	}

	// join pthreads
	for(i = 1; i < 3; i++)
	{
       pthread_join(thread[i], NULL);
	}
	
	pthread_mutex_destroy(&key);
}