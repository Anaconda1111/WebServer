#ifndef THREAD_H__
#define THREAD_H__

#include <pthread.h>


class Thread{

public:
    Thread();
    ~Thread();
    virtual void run()=0;
    bool start();
    bool join();
    void exit();

private:
    pthread_t m_thread_id;
};


#endif