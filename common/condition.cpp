#include"condition.h"
#include <sys/time.h>
condition::condition(){
    pthread_cond_init(&m_cond,NULL);
}

condition::~condition(){
    pthread_cond_destroy(&m_cond);
}

void condition::Wait(Mutex* mutex){
    pthread_cond_wait(&m_cond,mutex->getMutex());
}

void condition::Signal(){
    pthread_cond_signal(&m_cond);
}

void condition::BroadCast(){
    pthread_cond_broadcast(&m_cond);
}

int condition::timeWait(Mutex* mutex,int ms){
    if(ms<0){
        Wait(mutex);
        return 0;
    }

    timespec outTime;
    struct timeval now;
    gettimeofday(&now,NULL);
    unsigned long usec=now.tv_usec+ms*1000UL;
    outTime.tv_sec=now.tv_sec+usec/1000000;
    outTime.tv_nsec=(usec%1000000)*1000;
    pthread_cond_timedwait(&m_cond,mutex->getMutex(),&outTime);
}