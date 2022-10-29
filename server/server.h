#ifndef SERVER_H__
#define SERVER_H__

#include "client/client.h"
#include "common/thread_pool.h"
#include "common/timer.h"
#include "database/mysql_pool.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <string>

class Server{

private:

    static const int MAX_EVENTS=10000;
    static const int MAX_CONNECT=65536;

    int epoll_fd;
    int listen_fd;
    bool is_init;
    int port;
    
    epoll_event events[MAX_EVENTS];
    
    static std::unordered_map<std::string,std::string> user_dict;
    void initUserDict();

    SequenceTimer timer;
    static bool time_out;
    static time_t overtime; 

    MysqlConnectionPool sql_pool;

    ThreadPool thread_pool;

    std::unordered_map<int,Client*> socket_to_client;

public:

    Server(int);
    ~Server();
    void newClientConnectHandle();       //处理客户端连接事件
    void readEvent(int socket_fd);       
    void writeEvent(int socket_fd);      //发生读事件
    void startListen();                  //创建socket对象并开始监听链接
    void startEpollLoop();               //开始epoll循环
    void init();                         //初始化各个模块，例如线程池、sql链接池等
    void timeoutEventHadnle();           //将数据库中用户表的信息读取到内存，保存在哈希表里
    static void alarmSignalHandle(int);  //信号处理函数，负责把time out变量赋值为true

};

#endif