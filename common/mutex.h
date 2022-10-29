#ifndef MUTEX_H__
#define MUTEX_H__

#include<pthread.h>

class Mutex{
private:
    pthread_mutex_t mutex;
public:
    //构造函数
    Mutex(){
        pthread_mutex_init(&mutex,NULL);
    }

    //析构函数
    ~Mutex(){
        pthread_mutex_destroy(&mutex);
    }

    void Lock(){
        pthread_mutex_lock(&mutex);
    }

    void UnLock(){
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_t* getMutex(){
        return &mutex;
    }

};

class AutoLock{
private:
    Mutex* ptrToMutex;
public:
    AutoLock(Mutex* p):ptrToMutex(p){
        ptrToMutex->Lock();
    }

    ~AutoLock(){
        ptrToMutex->UnLock();
    }
};



















#endif
