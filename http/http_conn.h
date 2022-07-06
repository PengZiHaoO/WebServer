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
#include <mysql/mysql.h>
#include <fstream>

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
    //definition of 8 http code
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
    HTTPConnection(){

    }
    ~HTTPConnection(){

    }

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
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    char* get_line();
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content, ...);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL* m_mysql;
    int m_state;

private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    CHECK_STATE m_check_state;
    METHOD m_method;
    char m_real_file[FILE_NAME_LEN];
    char* m_url;
    char* m_version;
    char* m_host;
    int m_content_length;
    bool m_linger;
    char* m_file_address;
    struct stat m_file_stat;
    iovec m_iv[2];
    int m_iv_count;
    int cgi;
    char* m_string;
    int bytes_to_send;
    int bytes_have_send;
    char* doc_root;
    std::map<std::string, std::string> m_users;
    int m_trig_mode;
    int m_close_log;
    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif