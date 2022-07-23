#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include "../locker/locker.hpp"
#include "../dbconnpool/mysql_conn_pool.h"

template<class T>
class ThreadPool{
public:
    ThreadPool(int actor_model,ConnectionPool *conn_pool,int thread_number = 8,int max_request_number = 10000);
    ~ThreadPool();
    bool append(T* request, int state);
    bool append_p(T* request);
private:   
    static void* worker(void* args);
    void run();
private:
    int m_thread_number;
    int m_max_requests_number;
    pthread_t* m_threads;
    std::list<T* >m_worker_queue;
    Mutex m_queue_mutex;
    Semaphore m_queue_state;
    ConnectionPool* m_conn_pool;
    int m_actor_model;
};

#endif