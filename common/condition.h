#ifndef CONDITION_H__
#define CONDITION_H__
#include "mutex.h"
class condition
{
private:
pthread_cond_t m_cond;
public:

    condition();

    ~condition();

    void Wait(Mutex*);

    void Signal();

    void BroadCast();

    int timeWait(Mutex*,int);
};





#endif