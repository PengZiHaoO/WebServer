#ifndef TIMER_H
#define TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

class UtilityTimer;

//客户端连接资源
struct ClientData {
    sockaddr_in address;
    int sockfd;
    UtilityTimer *m_timer;
};

//定时器类
class UtilityTimer
{
public:
    //超时时间
    time_t expire;

    //定时事件回调函数
    void (* timer_event)(ClientData *);
    //连接资源
    ClientData *user_data;
    //定时器前序和后续节点
    UtilityTimer *prev;
    UtilityTimer *next;
public:
    UtilityTimer(): prev(nullptr), next(nullptr){
        
    }
    ~UtilityTimer(){

    }
};

//定时器容器类
//通过双向有序链表定时器组织起来
class TimerContainer{
public:
    TimerContainer();
    ~TimerContainer();

    void add_timer_node(UtilityTimer *timer_node);
    void adjust_timer_node(UtilityTimer *timer_node);
    void delete_timer_node(UtilityTimer *timer_node);
    void timer_tick();
private:
    void add_timer_node(UtilityTimer *timer_node, UtilityTimer *head);

    UtilityTimer *m_head;
    UtilityTimer *m_tail;
};

//一些需要的方法
class Utility{
public:
    Utility(){

    }
    ~Utility(){

    }

    void init(int timeslot);
    int setnonblocking(int fd);
    void addfd(int epollfd, int fd, bool one_shoot, int trig_mode);
    static void sig_handler(int sig);
    void add_sig(int sig, void(sig_handler)(int), bool start = true);
    void timer_handler();
    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    TimerContainer m_timer_container;
    static int u_epollfd;
    int m_timeslot;
};

//被调用的
void timer_event(ClientData *user_data);

#endif