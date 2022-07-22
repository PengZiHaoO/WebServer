#include "mysql_conn_pool.h"

//从连接池中获取一个连接
//即从list中取出一个并pop
MYSQL *ConnectionPool::get_connection(){
    MYSQL *conn = nullptr;

    if(0 == conn_list.size()){
        return nullptr;
    }

    reserve.wait();

    m_mutex.lock();

    conn = conn_list.front();
    conn_list.pop_front();

    ++m_curr_conn;
    --m_free_conn;

    m_mutex.unlock();

    return conn;
}

//释放连接
//即放回连接池
bool ConnectionPool::release_connection(MYSQL *conn){
    if(nullptr == conn){
        return false;
    }

    m_mutex.lock();

    conn_list.push_back(conn);

    --m_curr_conn;
    ++m_free_conn;

    m_mutex.unlock();

    reserve.post();

    return true;
}

//返回空闲连接数量
int ConnectionPool::get_free_conn(){
    return this->m_free_conn;
}

//置空连接池
//即list中元素置空释放list
void ConnectionPool::destroy_pool(){
    m_mutex.lock();

    if(conn_list.size() > 0){
        std::list<MYSQL *>::iterator iter;

        for(iter = conn_list.begin(); iter != conn_list.end(); ++iter){
            MYSQL *temp = *iter;
            mysql_close(temp);
        }

        m_curr_conn = 0;
        m_free_conn = 0;

        conn_list.clear();
    }

    m_mutex.unlock();
}

//单例模式 懒汉实现
ConnectionPool *ConnectionPool::get_instance(){
    static ConnectionPool instance;

    return &instance;
}

void ConnectionPool::init(std::string url, std::string user, std::string passwd, std::string database_name, int port, int close_log){
    m_url = url;
    m_user = user;
    m_password = passwd;
    m_database_name = database_name;
    m_port = port;
    m_close_log = close_log;

    for(int i = 0; i < m_max_conn; ++i){
        MYSQL *conn = nullptr;
        conn = mysql_init(conn);

        if(conn == nullptr){
            LOG_ERROR("MySQL Error");
            exit(1);
        }

        conn = mysql_real_connect(conn, url.c_str(), user.c_str(), passwd.c_str(), database_name.c_str(), port, nullptr, 0);

        if(conn == nullptr){
            LOG_ERROR("MySQL ERROR");
            exit(1);
        }

        conn_list.push_back(conn);

        ++m_free_conn;
    }

    reserve = Semaphore(m_free_conn);
    
    m_max_conn = m_free_conn;
}

ConnectionPool::ConnectionPool(){
    m_curr_conn = 0;
    m_free_conn = 0;
    m_close_log = 0;
    m_max_conn = 0;
}

ConnectionPool::~ConnectionPool(){
    destroy_pool();
}



ConnectionRAII::ConnectionRAII(MYSQL **conn, ConnectionPool *conn_pool){
    *conn = conn_pool->get_connection();

    conn_RAII = *conn;
    pool_RAII = conn_pool;
}

ConnectionRAII::~ConnectionRAII(){
    pool_RAII->release_connection(conn_RAII);
}