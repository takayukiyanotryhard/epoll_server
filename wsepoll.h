#pragma once

#ifndef _WSEPOLL_H_
#define _WSEPOLL_H_

#include <winsock2.h>

#define EPOLLIN      (FD_READ | FD_ACCEPT | FD_CLOSE)
#define EPOLLOUT     (FD_WRITE | FD_CONNECT | FD_CLOSE)
#define EPOLLRDHUP   (FD_CLOSE)
#define EPOLLPRI     (FD_OOB)
#define EPOLLERR     0
#define EPOLLHUP     (FD_CLOSE)
#define EPOLLET      0
#define EPOLLONESHOT 0

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

struct epoll_data {
    int fd;
};

struct epoll_event {
    unsigned int events;
    struct epoll_data data;
};

int epoll_create(int size);
int epoll_ctl(int epfd, int op, SOCKET fd, struct epoll_event* event);
int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);

#endif