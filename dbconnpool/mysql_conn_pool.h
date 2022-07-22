#ifndef MYSQL_CONN_POOL_H
#define MYSQL_CONN_POOL_H

#include <cstdio>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <cstring>
#include <string>
#include <cstdlib>

#include "../locker/locker.hpp"
#include "../log/log.h"

class ConnectionPool{
public:

    MYSQL *get_connection();

    bool release_connection(MYSQL *conn);

    int get_free_conn();

    void destroy_pool();

    //单例模式 懒汉实现
    static ConnectionPool *get_instance();

    void init(std::string url, std::string user, std::string passwd, std::string database_name, int port, int close_log);

private:

    ConnectionPool();

    ~ConnectionPool();

private:

    int m_max_conn;
    int m_curr_conn;
    int m_free_conn;

    Mutex m_mutex;
    std::list<MYSQL *> conn_list;
    Semaphore reserve;

public:

    std::string m_url;
    std::string m_port;
    std::string m_user;
    std::string m_password;
    std::string m_database_name;
    int m_close_log;

};

//利用类实现数据库连接的RAII和释放
class ConnectionRAII{
public:

    ConnectionRAII(MYSQL **conn, ConnectionPool *conn_pool);
    ~ConnectionRAII();

private:

    MYSQL *conn_RAII;
    ConnectionPool *pool_RAII;

};

#endif