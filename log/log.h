#ifndef LOG_H__
#define LOG_H__

#include "common/blocking_queue.h"
#include "common/mutex.h"
#include "common/thread.h"
#include <string>
#include <stdarg.h>


#define LOGGER_BUFFER_SIZE 300

typedef enum{
    TRACE=0,
    DEBUG,
    INFO,
    WARING,
    ERROR
}LOG_PRIORITY;


class Logger{
private:

    class Logger_Thread:public Thread{
    public:
        bool m_exit;
        bool m_waiting;
        BlockQueue<std::string>* ptr_bq;
        explicit Logger_Thread(BlockQueue<std::string>* ptr=nullptr):m_exit(false),m_waiting(true),ptr_bq(ptr){};
        void Terminal(bool waiting){m_waiting=waiting;m_exit=true;}
        virtual void run();
    
    };

    Logger():fd(-1),tail_ptr(0),is_init(false){m_thread.ptr_bq=&logger_task;};
    ~Logger();
    Logger(const Logger& rhs);

    void Write(const char* fmt,va_list);

    static void Write(LOG_PRIORITY,const char*,va_list);

    void Write(std::string *);

    Logger_Thread m_thread;

    int fd;                                 //打开log文件的文件描述符

    int tail_ptr;                           // 指向当前缓冲区末尾字符的位置

    char buffer[LOGGER_BUFFER_SIZE];        //缓冲区

    BlockQueue<std::string> logger_task;    //阻塞队列存储要写入的日志信息

    static LOG_PRIORITY level;              //日志等级

    bool is_init;

public:

    friend class Logger_Thread;

    void logger_init(const char* file_path,LOG_PRIORITY level);

    static void Debug(const char*fmt,...);

    static void Info(const char* fmt,...);

    static void Trace(const char* fmt,...);

    static void Warning(const char* fmt,...);

    static void Error(const char*fmt,...);

    static Logger& getInstance();
};


#endif