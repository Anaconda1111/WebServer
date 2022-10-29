#include "server.h"
#include "config/config.h"
#include "log/log.h"
#include <functional>
#include <strings.h>
#include <string.h>
#include <signal.h>
std::unordered_map<std::string,std::string> Server::user_dict;
bool Server::time_out=false;
time_t Server::overtime=Config::over_time;

Server::Server(int port):is_init(false),port(port),
                sql_pool(Config::mysql_addr,Config::mysql_user,
                Config::mysql_password,Config::mysql_dbname,Config::mysql_port,Config::mysql_max_conn),
                timer(){
}

Server::~Server(){
    for(auto it=socket_to_client.begin();it!=socket_to_client.end();++it){
        delete it->second;
    }
}

void Server::alarmSignalHandle(int signal){
    Server::time_out=true;
}

void Server::initUserDict(){
    MysqlConnectionRAII mysql_connect(&sql_pool);
    if(mysql_query(mysql_connect.real_ptr(),"SELECT name,password FROM user")==0){
        MYSQL_RES* data=mysql_store_result(mysql_connect.real_ptr());
        while(MYSQL_ROW row=mysql_fetch_row(data)){
            std::string name=row[0];
            std::string password=row[1];
            Server::user_dict[name]=password;
            Logger::Info("Server.initUserDict name=%s,password=%s",name.c_str(),user_dict[name].c_str());
        }
        mysql_free_result(data);
    }else{
        perror(mysql_error(mysql_connect.real_ptr()));
    }
}

void Server::init(){
    epoll_fd=epoll_create(10);
    assert(epoll_fd>0);

    Logger& logger=Logger::getInstance();
    logger.logger_init(Config::log_file_path.c_str(),ERROR);

    sql_pool.init();
    initUserDict();

    thread_pool.Init(Config::thread_pool_max_thread);

    Client::user_dict=&Server::user_dict;
    
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_flags |= SA_RESTART;
    sa.sa_handler=Server::alarmSignalHandle;
    sigfillset(&sa.sa_mask);
    sigaction(SIGALRM,&sa,NULL);

    Logger::Info("Server.init epoll fd=%d",epoll_fd);

    is_init=true;
}

void Server::startListen(){
    assert(is_init);
    listen_fd=socket(AF_INET,SOCK_STREAM,0);
    
    sockaddr_in server_addr;
    bzero(&server_addr,sizeof(server_addr));

    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    server_addr.sin_port=htons(port);
    
    int flag=1;
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));

    bind(listen_fd,(sockaddr*)&server_addr,sizeof(server_addr));
    listen(listen_fd,10);

    setFileNonBlock(listen_fd);
    epoll_add(epoll_fd,listen_fd,false);

    Logger::Info("Server.startListen lfd=%d",listen_fd);

}

void Server::newClientConnectHandle(){

    sockaddr_in client_addr;
    socklen_t   len=sizeof(client_addr);
    while(true){
        int cfd=accept(listen_fd,(sockaddr*)&client_addr,&len);
        if(cfd<0) break;
        setFileNonBlock(cfd);
        if(!socket_to_client.count(cfd)){
            socket_to_client[cfd]=new Client(epoll_fd,cfd);
        }
        Client* ptr_client=socket_to_client[cfd];
        int64_t timer_id = timer.AddTimer(std::bind(&Client::closeConnection,ptr_client),Server::overtime);
        
        int port=ntohs(client_addr.sin_port);

        ptr_client->init(&sql_pool,&timer,timer_id,cfd,port);
        epoll_add(epoll_fd,cfd,true);
        Logger::Info("Server.newClientConnectHandle client socket fd=%d,port=%d",cfd,port);
    }
}

void Server::readEvent(int socket_fd){
    Client* ptr_client=socket_to_client[socket_fd];
    thread_pool.AddTask(std::bind(&Client::readEventHandle,ptr_client));
    ptr_client->flushEventHapenTime();
    timer.ModTimer(ptr_client->getTimerID(),Config::over_time);
}

void Server::writeEvent(int socket_fd){
    Client* ptr_client=socket_to_client[socket_fd];
    thread_pool.AddTask(std::bind(&Client::writeEventHandle,ptr_client));
    ptr_client->flushEventHapenTime();
    //timer.ModTimer(ptr_client->getTimerID(),Config::over_time);
}

void Server::startEpollLoop(){
    alarm(Config::alarm_time);
    int cnt=0;
    while(true){
        cnt=epoll_wait(epoll_fd,events,MAX_EVENTS,-1);
        if(cnt<0 && errno!=EINTR) break;
        for(int i=0;i<cnt;i++){
            int socket_fd=events[i].data.fd;
            if(socket_fd==listen_fd){
                newClientConnectHandle();
            }else if(events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)){
                socket_to_client[socket_fd]->closeConnection();
            }else if(events[i].events & EPOLLIN){
                readEvent(socket_fd);
            }else if(events[i].events & EPOLLOUT){
                writeEvent(socket_fd);
            }
        }
        if(time_out){
            timeoutEventHadnle();
        }
    }
}

void Server::timeoutEventHadnle(){
    time_out=false;
    //timer.Update();
    while(true){
        srand((unsigned int)time(NULL));
        int count=0;
        for(int i=0;i<Config::client_timeout_check_nums&&socket_to_client.size();i++){
            auto random_it = std::next(std::begin(socket_to_client),rand()%socket_to_client.size());
            Client* random_client=random_it->second;
            if(!random_client->checkConnectionValid()){
                continue;
            }
            if(random_client->isOverTime()){
                random_client->closeConnection();
                count++;
            }
        }
        if(count>=Config::client_timeout_check_continue_threshold){
            continue;
        }
        break;
    }
    alarm(Config::alarm_time);
    //Logger::Info("Server.timeoutEventHadnle alarm time=%d",Config::alarm_time);
}