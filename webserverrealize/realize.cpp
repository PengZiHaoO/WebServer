#include "realize.h"

WebServer::WebServer(){
    users = new HTTPConnection[MAX_FD];

    char server_path[200];
    getcwd(server_path, 200);

    char root[6] = "/root";

    m_root = new char[strlen(server_path) + strlen(root) + 1];

    strcpy(m_root, server_path);
    strcat(m_root, root);

    user_timer = new ClientData[MAX_FD];
}

WebServer::~WebServer(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);

    delete[] m_root;
    delete[] user_timer;
    delete[] m_root;
    delete[] m_pool;
    delete[] user_timer;
}

void WebServer::init(int port , std::string user, std::string password, std::string database_name,
              int log_write , int opt_linger, int trig_mode, int sql_num,
              int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_password = password;
    m_database_name = database_name;
    m_sql_conn_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_opt_linger = opt_linger;
    m_trig_mode = trig_mode;
    m_close_log = close_log;
    m_actor_model = actor_model;
}