#ifndef  THREAD_POOL_H__
#define  THREAD_POOL_H__

#include "thread.h"
#include "blocking_queue.h"
#include <functional>
#include <vector>


class ThreadPool{

public:

    struct Status{
        Status():m_pending_num(0),m_working_num(0){};
        size_t m_pending_num;
        size_t m_working_num; 
    };
    

    enum Mode{
        NO_PENDING=0,
        PENDING
    };

    ThreadPool();
    ~ThreadPool();

    int Init(unsigned int thread_num=4,int mode=PENDING);

    int AddTask(std::function<void()>func,int task_id=-1);

    void GetStatus(Status*);

    void Terminate(bool waiting=true);

    bool GetFinishTaskID(int* task_id);

private:

    struct Task{
        std::function<void()> func;
        int task_id;
    };
    
    class InnerThread:public Thread{
    
    public:
        InnerThread(BlockQueue<Task>*pending_queue,BlockQueue<int>*work_queue,BlockQueue<int>*finish_queue);
        virtual void run();
        void Terminal(bool waiting=true);
    private:

        BlockQueue<Task>* m_pending_queue;
        BlockQueue<int>*  m_working_queue;
        BlockQueue<int>*  m_finish_queue;
        bool m_exit;
        bool m_waiting;
    };

    std::vector<InnerThread*> m_threads;
    BlockQueue<Task> m_pending_queue;
    BlockQueue<int>  m_working_queue;
    BlockQueue<int>  m_finish_queue;
    bool m_initialized;
    bool m_exit;
    unsigned int m_thread_num;
    int m_mode;
};


#endif