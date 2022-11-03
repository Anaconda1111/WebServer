#include"util.h"

int setFileNonBlock(int fd){
    int old_flag=fcntl(fd,F_GETFL);
    int new_flag=old_flag | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_flag);
    return old_flag;
}

void epoll_add(int epoll_fd,int fd,bool one_shoot=true){
    epoll_event ev;
    ev.data.fd=fd;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP;
    if(one_shoot){
        ev.events|=EPOLLONESHOT;
    }
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&ev);
}

void epoll_remove(int epoll_fd,int fd){
    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,NULL);
}

void epoll_mod(int epoll_fd,int fd,int event,bool one_shoot=true){
    epoll_event ev;
    ev.data.fd=fd;
    ev.events= event | EPOLLRDHUP | EPOLLHUP | EPOLLET;
    if(one_shoot){
        ev.events |= EPOLLONESHOT;
    }
    epoll_ctl(epoll_fd,EPOLL_CTL_MOD,fd,&ev);
}