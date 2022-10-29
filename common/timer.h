#ifndef TIMER_H__
#define TIMER_H__

#include<functional>
#include<unordered_map>
#include<list>
#include<memory>
#include<stdint.h>
#include"mutex.h"

typedef std::function<int ()> TimerCallbackFunction;

class Timer{

public:

    virtual ~Timer() {};

    virtual int64_t AddTimer(TimerCallbackFunction&&,time_t out_time)=0;

    virtual int64_t ModTimer(int64_t timer_id,time_t new_out_time)=0;

    virtual int StopTimer(int64_t timer_id)=0;

    virtual int Update()=0;

    virtual int GetTimerNum(){return 0;}

};


class SequenceTimer:public Timer{

public:

    SequenceTimer();

    virtual ~SequenceTimer();

    virtual int64_t AddTimer(TimerCallbackFunction&&,time_t out_time);

    virtual int64_t ModTimer(int64_t timer_id,time_t new_out_time);

    virtual int StopTimer(int64_t timer_id);

    virtual int Update();

    virtual int GetTimerNum(){return id_2_timer.size();}

private:

    struct TimerItem{
        bool is_stop;
        time_t time_out;
        int64_t id;
        TimerCallbackFunction cb;
        TimerItem(){
            is_stop=false;
            id=-1;
            time_out=0;
        }

    };
    
    int64_t get_id();

    int64_t timer_seq_id;
    
    std::unordered_map< time_t,std::list< std::shared_ptr<TimerItem> > > timer_list;
    std::unordered_map< int64_t,std::shared_ptr<TimerItem>> id_2_timer;

    Mutex seq_timer_lock;

};

class SortTimer:public Timer{

public:
    SortTimer(time_t);

    virtual ~SortTimer();

    virtual int64_t AddTimer(TimerCallbackFunction&&,time_t=0);

    virtual int64_t ModTimer(int64_t,time_t=0);

    virtual int Update();

    virtual int StopTimer(int64_t);

    virtual int GetTimerNum(){return timer_list.size();};

private:

    int64_t get_id();

    struct TimerItem{
        bool is_stop;
        time_t time_out;
        int64_t id;
        TimerCallbackFunction cb;
        TimerItem(){
            is_stop=false;
            id=-1;
            time_out=0;
        }

    };
    
    const time_t  out_time;
    int64_t timer_sort_id;

    typedef std::list<std::shared_ptr<TimerItem>>::iterator lit;
    std::unordered_map<int64_t,lit> id_to_timer;
    std::list<std::shared_ptr<TimerItem>> timer_list;

    Mutex sort_timer_lock;

};

#endif