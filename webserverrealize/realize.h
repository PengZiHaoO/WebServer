#ifndef REALIZE_H
#define REALIZE_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <cstdlib>
#include <cassert>
#include <sys/epoll.h>

#include "../threadpool/threadpool.h"
#include "../http/http_conn.h"
#include "../timer/timer.h"

const int MAX_FD = 65536;
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT = 5;//5sec

class WebServer{
public:

    WebServer();
    ~WebServer();

public:

    void init(int port , std::string user, std::string password, std::string database_name,
              int log_write , int opt_linger, int trig_mode, int sql_num,
              int thread_num, int close_log, int actor_model);

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void event_listen();
    void event_loop();
    void timer(int connfd, sockaddr_in client_address);
    void adjust_timer(UtilityTimer *timer);
    void deal_timer(UtilityTimer *timer, int sockfd);
    bool deal_clientdata();
    bool deal_signal(bool &timeout, bool &stop_server);
    void deal_read(int sockfd);
    void deal_write(int sockfd);

public:

    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actor_model;

    int m_pipefd[2];//管程
    int m_epollfd;

    HTTPConnection *users;

    ConnectionPool *m_conn_pool;
    std::string m_user;
    std::string m_password;
    std::string m_database_name;
    int m_sql_conn_num;

    ThreadPool<HTTPConnection> *m_pool;
    int m_thread_num;

    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_opt_linger;
    int m_trig_mode;
    int m_listen_trig_mode;
    int m_conn_trig_mode;

    Utility utils;
    ClientData *user_timer;
};
#endif