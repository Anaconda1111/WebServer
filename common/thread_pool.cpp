#include "thread_pool.h"

ThreadPool::ThreadPool():m_exit(false),m_initialized(false),m_thread_num(0),m_mode(PENDING){}

ThreadPool::~ThreadPool(){
    Terminate();
}


int ThreadPool::Init(unsigned int thread_num,int mode){
    
    if(m_initialized)
        return -1;

    if(thread_num>256)
        thread_num=256;

    m_thread_num=thread_num;
    if(mode>=NO_PENDING||mode<=PENDING){
        m_mode=mode;
    }
    
    for(int i=0;i<thread_num;i++){
        InnerThread* temp=new InnerThread(&m_pending_queue,&m_working_queue,&m_finish_queue);
        m_threads.push_back(temp);
        temp->start();
    }

    m_initialized=true;
    return 0;
}


int ThreadPool::AddTask(std::function<void()>func,int task_id){
    if(!m_initialized)
        return -1;

    if(m_mode==NO_PENDING){
        Status stat;
        GetStatus(&stat);
        if(stat.m_working_num+stat.m_pending_num>m_thread_num){
            return -2;
        }
    }

    Task t;
    t.func=func;
    t.task_id=task_id;
    m_pending_queue.push_back(t);
    return 0;
}


void ThreadPool::GetStatus(Status* s){
    if(s==NULL){
        return;
    }
    s->m_pending_num=m_pending_queue.size();
    s->m_working_num=m_working_queue.size();
}

void ThreadPool::Terminate(bool waiting){
    m_exit=true;
    for(int i=0;i<m_threads.size();i++){
        m_threads[i]->Terminal(waiting);
    }

    for(int i=0;i<m_threads.size();i++){
        m_threads[i]->join();
        delete m_threads[i];
    }
    m_pending_queue.clear();
    m_working_queue.clear();
    m_finish_queue.clear();
}


bool ThreadPool::GetFinishTaskID(int* task_id){
    if(m_finish_queue.is_empty()||task_id==nullptr)
        return false;
    bool ret=m_finish_queue.time_pop_front(task_id,100);
    return ret;
}


ThreadPool::InnerThread::InnerThread(BlockQueue<Task>*pending_queue,BlockQueue<int>*work_queue,BlockQueue<int>*finish_queue){
    m_working_queue=work_queue;
    m_pending_queue=pending_queue;
    m_finish_queue=finish_queue;
    m_exit=false;
    m_waiting=true;
}


void ThreadPool::InnerThread::run(){
    while(1){
        if(m_exit && (m_waiting==false || m_pending_queue->is_empty())){
            break;
        }
        Task t;
        bool ret=m_pending_queue->time_pop_front(&t,1000);
        if(ret){
            m_working_queue->push_back(0);
            t.func();
            if(t.task_id>=0){
                m_finish_queue->push_back(t.task_id);
            }
            int temp;
            m_working_queue->try_pop_front(&temp);
        }
    }
}

void ThreadPool::InnerThread::Terminal(bool waiting){
    m_exit=true;
    m_waiting=waiting;
}

