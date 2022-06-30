#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <semaphore.h>
#include <exception>


class Semaphore{
private:
    sem_t m_sem;
public:
    Semaphore(int num = 0){
        if(sem_init(&m_sem, 0, num)){
            throw std::exception();
        }
    }

    ~Semaphore(){
        sem_destroy(&m_sem);
    }

    bool wait(){
        return sem_wait(&m_sem) == 0;
    }

    bool post(){ 
        return sem_post(&m_sem) == 0;
    }
};

class Mutex{
private:
    pthread_mutex_t m_mutex;
public:
    Mutex(){
        if(pthread_mutex_init(&m_mutex, NULL)){
            throw std::exception();
        }
    }

    ~Mutex(){
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t* get(){
        return &m_mutex;
    }
};

class Condition{
private:
    pthread_cond_t m_cond;
public:
    Condition(){
        if(pthread_cond_init(&m_cond, NULL) == 0){
            throw std::exception();
        }
    }

    ~Condition(){
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t* mutex){
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, mutex);
        return ret == 0;
    }

    bool time_wait(pthread_mutex_t* mutex, const timespec* t){
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, mutex, t);
        return ret == 0;
    }

    bool broadcast(){
        return pthread_cond_broadcast(&m_cond);
    }
    
    bool signal(){   
        return pthread_cond_signal(&m_cond);
    }
};

#endif