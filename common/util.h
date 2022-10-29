#ifndef UTIL_H__
#define UTIL_H__
#include<fcntl.h>
#include<unistd.h>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<sys/stat.h>

int setFileNonBlock(int fd);

void epoll_add(int epoll_fd,int fd,bool one_shoot);

void epoll_remove(int epoll_fd,int fd);

void epoll_mod(int epoll_fd,int fd,int event,bool one_shoot);


#endif