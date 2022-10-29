#include "timer.h"
#include "time_util.h"
#include "log/log.h"
SequenceTimer::SequenceTimer(){
    timer_seq_id=0;
}

SequenceTimer::~SequenceTimer(){
}


int64_t SequenceTimer::get_id(){
    
    int64_t old_id;
    {
        AutoLock locker(&seq_timer_lock);
        old_id=timer_seq_id;
        timer_seq_id++;        
    }
    return old_id;
}

int64_t SequenceTimer::AddTimer(TimerCallbackFunction&& cb,time_t out_time){
    if(!cb || 0==out_time){
        Logger::Info("timer.cpp AddTimer: invalid param");
        return -1;
    }
    std::shared_ptr<TimerItem> ptr(new TimerItem);
    ptr->cb=cb;
    ptr->time_out=out_time+now_sec();
    ptr->id=get_id();
    {
        AutoLock locker(&seq_timer_lock);
        id_2_timer[ptr->id]=ptr;
        timer_list[out_time].push_back(ptr);
    }
    return ptr->id;
}
int64_t SequenceTimer::ModTimer(int64_t id,time_t time_out){
    {
        AutoLock locker(&seq_timer_lock);
        if(id_2_timer.count(id)==0){
            Logger::Info("ModTimer  Timer Not Exist,id=%lld",id);
            return id;
        }
        auto it=id_2_timer.find(id);
        it->second->time_out=now_sec()+time_out;

        for(auto it=timer_list[time_out].begin();it!=timer_list[time_out].end();++it){
            if((*it)->id==id){
                timer_list[time_out].erase(it);
                break;
            }
        }
        timer_list[time_out].push_back(it->second);
    }
    Logger::Info("Mod Timer id=%lld,out_time=%ld",id,time_out);
    return id;
}

int SequenceTimer::StopTimer(int64_t id){
    {
        AutoLock locker(&seq_timer_lock);
        auto it=id_2_timer.find(id);
        if(it==id_2_timer.end()){
            Logger::Info("StopTimer Timer Not Exist,id=%ld",id);
            return -1; 
        }
        it->second->is_stop=true;
        id_2_timer.erase(it);
    }
    Logger::Info("StopTimer id=%ld",id);
    return 1;
}

int SequenceTimer::Update(){

    int ret=0;
    int num=0;
    time_t now=now_sec();
    time_t old_time;
    {
        AutoLock locker(&seq_timer_lock);

        auto it=timer_list.begin();
        while (it!=timer_list.end()){

            auto& list=it->second;
            auto lit=list.begin();

            while(!list.empty()){
                
                lit=list.begin();

                if((*lit)->is_stop){
                    list.erase(lit);
                    continue;
                }
                
                if(now<(*lit)->time_out){
                    break;
                }

                old_time=it->first;
                ret=(*lit)->cb();
                if(ret<0){
                    id_2_timer.erase((*lit)->id);
                    list.erase(lit);
                    Logger::Info("Timer Update: delete timer id=%lld",(*lit)->id);
                }else{
                    auto back=*lit;
                    time_t new_time= ret==0?old_time:ret;
                    back->time_out=now+new_time;
                    list.erase(lit);
                    timer_list[new_time].push_back(back);
                    Logger::Info("Timer Update: flush timer id=%lld,out_time=%ld",back->id,back->time_out);
                }
                num++;
            }
            ++it;
        }

        for(auto it=timer_list.begin();it!=timer_list.end();++it){
            if(it->second.size()==0){
                Logger::Info("Timer.Update: delete timer list delay=%ld",it->first);
                timer_list.erase(it);
            }
        }
        
    }
    return num;
}

//------------------------------------------------------------------------------------------------------------------

SortTimer::SortTimer(time_t out_time):out_time(out_time),timer_sort_id(0){

}

SortTimer::~SortTimer(){

}

int64_t SortTimer::get_id(){
    AutoLock locker(&sort_timer_lock);
    int64_t old_value=timer_sort_id;
    timer_sort_id++;
    return old_value;
}

int64_t SortTimer::AddTimer(TimerCallbackFunction&& cb,time_t _){
    if(!cb){
        Logger::Info("SortTimer.AddTimer: invalid param");
        return -1;
    }
    std::shared_ptr<TimerItem> ptr(new TimerItem);
    ptr->cb=cb;
    ptr->id=get_id();
    ptr->time_out=now_sec()+out_time;
    {
        AutoLock locker(&sort_timer_lock);
        timer_list.push_front(ptr);
        id_to_timer[ptr->id]=timer_list.begin();
    }
    Logger::Info("SortTimer.AddTimer: Add timer done! id=%lld, addr=%p",ptr->id,*(id_to_timer[ptr->id]));
    return ptr->id;
}

int64_t SortTimer::ModTimer(int64_t id,time_t _){
    if(!id_to_timer.count(id)){
        Logger::Info("SortTimer.ModTimer: timer not exist  id=%lld",id);
        return -1;
    }
    {
        AutoLock locker(&sort_timer_lock);
        lit it=id_to_timer[id];
        auto back=*it;
        timer_list.erase(it);
        back->time_out=now_sec()+out_time;
        timer_list.push_front(back);
        id_to_timer[id]=timer_list.begin();
        Logger::Info("SortTimer.ModTimer: Mod TImer Done! id=%lld,out_time=%ld",id,back->time_out);
    }
    return id;
}

int SortTimer::Update(){
    int cnt=0;
    Logger::Info("SortTimer.Update timer nums=%d",GetTimerNum());
    {
        AutoLock locker(&sort_timer_lock);
        while(timer_list.size()){
            
            auto ptr_to_timer=timer_list.back();
            time_t now=now_sec();
            if(ptr_to_timer->time_out>now){
                break;
            }

            lit it=id_to_timer[ptr_to_timer->id];
            
            int64_t id=ptr_to_timer->id;
            id_to_timer.erase(id);
            timer_list.erase(it);
            
            int res=ptr_to_timer->cb();

            if(res>0){
                ptr_to_timer->time_out=now+out_time;
                timer_list.push_front(ptr_to_timer);
                id_to_timer[id]=timer_list.begin();
            }else{
                Logger::Info("SortTimer.Update  delete timer id=%lld",id);
            }
            cnt++;
        }
        return cnt;
    }
}

int SortTimer::StopTimer(int64_t id){
    if(!id_to_timer.count(id)){
        Logger::Info("SortTimer.StopTimer: Timer not exist id=%lld",id);
        return -1;
    }
    AutoLock locker(&sort_timer_lock);
    lit it=id_to_timer[id];
    timer_list.erase(it);
    id_to_timer.erase(id);
    Logger::Info("SortTimer.StopTimer id=%lld",id);
    return id;
}