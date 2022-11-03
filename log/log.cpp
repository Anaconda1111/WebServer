#include "log.h"
#include"common/time_util.h"
#include<sys/stat.h>
#include<fcntl.h>
#include<assert.h>
#include<unistd.h>
#include<memory.h>
LOG_PRIORITY Logger::level=TRACE;

Logger::~Logger(){
    m_thread.Terminal(true);
    m_thread.join();
}

void Logger::logger_init(const char* file_path,LOG_PRIORITY priority){
    if(is_init)
        return;
    fd=open(file_path,O_RDWR | O_CREAT,0777);
    assert(fd>0);
    assert(m_thread.start());
    //assert(priority>=TRACE&&priority<=ERROR);
    ftruncate(fd,0);
    lseek(fd,0,SEEK_SET);
    Logger::level=priority;
    is_init=true;
}

void Logger::Write(const char* fmt,va_list args){
    char s[250]={0};
    tm _tm=get_specific_time();
    int n=0,m=0;
    n=snprintf(s,100,"%04d-%02d-%02d %02d:%02d:%02d ",_tm.tm_year+1900,_tm.tm_mon+1,_tm.tm_mday,_tm.tm_hour,_tm.tm_min,_tm.tm_sec);
    m=vsnprintf(s+n,100-n,fmt,args);
    s[n+m]='\n';
    s[n+m+1]='\0';
    std::string str=s;
    logger_task.push_back(str);
}

void Logger::Logger_Thread::run(){
    std::string str;
    while(1){
        if(m_exit && (m_waiting==false || ptr_bq->is_empty())){
            break;
        }
        ptr_bq->pop_front(&str);
        Logger& logger=Logger::getInstance();
        logger.Write(&str);
    }
}

void Logger::Write(LOG_PRIORITY l,const char*fmt,va_list args){
    if(Logger::level>l) return;
    Logger& logger=getInstance();
    logger.Write(fmt,args);
}



Logger& Logger::getInstance(){
    static Logger logger;
    return logger;
}

void Logger::Trace(const char* fmt,...){
    va_list args;
    va_start(args,fmt);
    Logger::Write(TRACE,fmt,args);
    va_end(args);
}

void Logger::Debug(const char* fmt,...){
    va_list args;
    va_start(args,fmt);
    Logger::Write(DEBUG,fmt,args);
    va_end(args);
}

void Logger::Info(const char* fmt,...){
    va_list args;
    va_start(args,fmt);
    Logger::Write(INFO,fmt,args);
    va_end(args);
}

void Logger::Warning(const char* fmt,...){
    va_list args;
    va_start(args,fmt);
    Logger::Write(WARING,fmt,args);
    va_end(args);
}

void Logger::Error(const char* fmt,...){
    va_list args;
    va_start(args,fmt);
    Logger::Write(ERROR,fmt,args);
    va_end(args);
}

void Logger::Write(std::string* p_str){
    int m=snprintf(buffer+tail_ptr,LOGGER_BUFFER_SIZE-tail_ptr,"%s",p_str->c_str());
    tail_ptr+=m;
    int temp=LOGGER_BUFFER_SIZE/10;
    if(/*true||*/tail_ptr>=6*temp){
        assert(write(fd,buffer,tail_ptr)!=-1);
        tail_ptr=0;
        memset(buffer,0,LOGGER_BUFFER_SIZE);
    }
}