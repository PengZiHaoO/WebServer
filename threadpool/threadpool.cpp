#include "threadpool.h"

template<class T>
ThreadPool<T>::ThreadPool(int actor_model,ConnectionPool *conn_pool, int thread_number /*= 8*/,int max_request_number /*= 10000*/)
                          :m_actor_model(actor_model),m_conn_pool(conn_pool),m_thread_number(thread_number),m_max_requests(max_requests_number),
                           m_threads(NULL){
    if(thread_number <= 0 || max_request_number <= 0){
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];

    if(!m_threads){
        delete[] m_threads;
        throw std::exception();
    }

    for(int i = 0; i < m_thread_number; ++i){
        if(pthread_create(m_threads + i, NULL, worker, this)){
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<class T>
ThreadPool<T>::~ThreadPool(){
    delete[] m_threads;
}

template<class T>
bool ThreadPool<T>::append(T* request,int state){
    m_queue_mutex.lock();

    if(m_worker_queue.size() >= m_max_requests_number){
        m_queue_mutex.unlock();
        return false;
    }

    requests->m_state = state;
    m_worker_queue.push_back(request);
    m_queue_mutex.unlock();
    m_queue_state.post();

    return true;
}

template<class T>
bool ThreadPool<T>::append_p(T* request){
    m_queue_mutex.lock();

    if(m_queue_mutex.size() >= m_max_requests_number){
        m_queue_mutex.unlock();
        retrun false;
    }

    m_worker_queue.push_back(request);
    m_queue_mutex.unlock();
    m_queue_state.post();

    return true;
}

template<class T>
void* ThreadPool<T>::worker(void* args){
    ThreadPool* thread = (ThreadPool *)args;
    thread->run();
    return thread;
}

template<class T>
void ThreadPool<T>::run(){

    while(true){
        m_queue_state.wait();
        m_queue_mutex.lock();

        if(m_worker_queue.empty()){
            m_queue_mutex.unlock();
            continue;
        }

        T* request = m_worker_queue.front();
        m_worker_queue.pop_front();
        m_queue_mutex.unlock();

        if(!request){
            continue;
        }
        
        if(1 == m_actor_model){

            if(0 == request->m_state){
                if(request->read_once()){
                    request->improv = 1;
                    ConnectionRAII mysql_conn(&request->mysql, m_conn_pool);
                    request->process();
                }
                else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else{
                if(request->write()){
                    request->improv = 1;
                }else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else{
            ConnectionRAII mysql_conn(&request->mysql, m_conn_pool);
            request->process();
        }
    }
}
