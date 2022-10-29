#ifndef BLOCKING_QUEUE__H
#define BLOCKING_QUEUE__H

#include "mutex.h"
#include "condition.h"
#include<deque>
#include<assert.h>
template <typename T>
class BlockQueue{

public:
    
    BlockQueue(size_t m=10000):max_size(m){
        assert(max_size>0);
    }


    void push_front(const T& new_item){
        {
            //尝试获取锁，autolock会在这个代码快内部自动加锁和解锁
            AutoLock locker(&m_mutex);
            while (unlock_isfull())
            {
                queue_not_full.Wait(&m_mutex);
            }
            m_queue.push_front(new_item);
        }
        queue_not_empty.Signal();
    }

    bool try_push_front(const T& new_item){
        {
            AutoLock locker(&m_mutex);
            if(unlock_isfull()){
                return false;
            }
            m_queue.push_front(new_item);
        }
        queue_not_empty.Signal();
        return true;
    }


    void push_back(const T& new_item){
        {
            AutoLock  locker(&m_mutex);
            while(unlock_isfull()){
                queue_not_full.Wait(&m_mutex);
            }
            m_queue.push_back(new_item);
        }
        queue_not_empty.Signal();
    }

    bool try_push_back(const T& new_item){
        {
            AutoLock locker(&m_mutex);
            if(unlock_isfull()){
                return false;
            }
            m_queue.push_back(new_item);
        }
        queue_not_empty.Signal();
        return true;
    }

    void pop_front(T*value){
        {
            AutoLock locker(&m_mutex);
            while(m_queue.empty()){
                queue_not_empty.Wait(&m_mutex);
            }
            *value=m_queue.front();
            m_queue.pop_front();
        }
        queue_not_full.Signal();
    }

    bool try_pop_front(T*value){
        {
            AutoLock locker(&m_mutex);
            if(m_queue.empty()){
                return false;
            }
            *value=m_queue.front();
            m_queue.pop_front();
        }
        queue_not_full.Signal();
        return true;
    }

    void pop_back(T*value){
        {
            AutoLock locker(&m_mutex);
            while (m_queue.empty()){
                queue_not_empty.Wait(&m_mutex);
            }
            *value=m_queue.back();
            m_queue.pop_back();
        }
        queue_not_full.Signal();
        
    }

    bool try_pop_back(T*value){
        {
            AutoLock locker(&m_mutex);
            if(m_queue.empty()){
                return false;
            }
            *value=m_queue.back();
            m_queue.pop_back();
        }
        queue_not_full.Signal();
        return true;
    }

    bool time_push_front(const T& new_item,int ms){
        bool res=false;
        {
            AutoLock locker(&m_mutex);
            if(unlock_isfull()){
                queue_not_full.timeWait(&m_mutex,ms);
            }
            if(!unlock_isfull()){
                m_queue.push_front(new_item);
                res=true;
            }
        }
        if(res)
            queue_not_empty.Signal();
        return res;
    }

    bool time_push_back(const T& new_item,int ms){
        bool res=false;
        {
            AutoLock locker(&m_mutex);
            if(unlock_isfull()){
                queue_not_full.timeWait(&m_mutex,ms);
            }
            if(!unlock_isfull){
                m_queue.push_back(new_item);
                res=true;
            }
        }
        if(res)
            queue_not_empty.Signal();
        return res;
    }

    bool time_pop_front(T*value,int ms){
        bool res=false;
        {
            AutoLock locker(&m_mutex);
            if(m_queue.empty()){
                queue_not_empty.timeWait(&m_mutex,ms);
            }
            if(!m_queue.empty()){
                *value=m_queue.front();
                m_queue.pop_front();
                res=true;
            }
        }
        if(res)
            queue_not_full.Signal();
        return res;
    }

    bool time_pop_back(T*value,int ms){
        bool res=false;
        {
            AutoLock locker(&m_mutex);
            if(m_queue.empty()){
                queue_not_empty.timeWait(&m_mutex,ms);
            }
            if(!m_queue.empty()){
                *value=m_queue.back();
                m_queue.pop_back();
                res=true;
            }
        }
        if(res)
            queue_not_full.Signal();
        return res;
    }

    bool is_empty(){
        AutoLock locker(&m_mutex);
        return m_queue.empty();
    }

    bool is_full(){
        AutoLock locker(&m_mutex);
        return m_queue.size()>=max_size;
    }

    void clear(){
        AutoLock locker(&m_mutex);
        m_queue.clear();
        queue_not_full.BroadCast();
    }


    size_t size(){
        AutoLock locker(&m_mutex);
        return m_queue.size();
    }


private:


    bool unlock_isfull(){
        return m_queue.size()>=max_size;
    }


    Mutex m_mutex;
    condition queue_not_full;
    condition queue_not_empty;
    size_t max_size;
    std::deque<T> m_queue;
};


#endif