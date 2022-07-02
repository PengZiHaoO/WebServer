#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../locker/locker.hpp"
//#include "../CGImysql/sql_connection_pool.h"
//#include "../timer/lst_timer.h"
//#include "../log/log.h"

class HTTPConnection{
public:
    //definition of some constant value with static which belong to the class 
    static const int FILE_NAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    //definition of 8 http method in enum
    enum METHOD{
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    //definition of 3 check states (not the blank)
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    //definition of 8 htto code
    enum HTTP_CODE{
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    //
    enum LINE_STATUS{
        LINE_OK,
        LINE_BAD,
        LINE_OPEN
    };

public:
    HTTPConnection();
    ~HTTPConnection();

public:
    void init(int sockfd, const sockaddr_in& addr, char*, int ,int ,std::string user, std::string passwd, std::string sqlname);
    void close_conn(bool real_close = 1);
    void process();
    bool read_once();
    bool write();
    sockaddr_in* get_address();
    void initmysql_result(/*ConnectionPool* conn_pool*/);
    int timer_flag;
    int improv;

private:
    
};
#endif