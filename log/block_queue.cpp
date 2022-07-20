#include "block_queue.h"

template<class T>
BlockQueue<T>::BlockQueue(int max_size/*= 1000*/){
    if(max_size <= 0){
        exit(-1);
    }

    m_max_size = max_size;
    m_array = new T[max_size];
    m_size = 0;
    m_front = -1;
    m_back = -1;
}

template<class T>
BlockQueue<T>::~BlockQueue(){
    m_mutex.lock();
    if(m_array != nullptr)
        delete[] m_array;
    m_mutex.unlock();
}

template<class T>
void BlockQueue<T>::clear(){
    m_mutex.lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.unlock();
}

template<class T>
bool BlockQueue<T>::full(){
    m_mutex.lock();
    if(m_size >= m_max_size){
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template<class T>
bool BlockQueue<T>::empty(){
    m_mutex.lock();
    if(0 == m_size){
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return flase;
}

template<class T>
bool BlockQueue<T>::front(T &val){
    m_mutex.lock();
    if(0 == m_size){
        m_mutex.unlock();
        return false;
    }
    val = m_array[m_front];
    m_mutex.unlock();
    return true;
}

template<class T>
bool BlockQueue<T>::back(T &val){
    m_mutex.lock();
    if(0 == m_size){
        m_mutex.unlock();
        return false;
    }
    val = m_array[m_back];
    m_mutex.unlock();
    return true;
}

template<class T>
int BlockQueue<T>::size(){
    m_mutex.lock();
    int temp = m_size;
    m_mutex.unlock();
    return temp;
}

template<class T>
int BlockQueue<T>::max_size(){
     m_mutex.lock();
    int temp = m_max_size;
    m_mutex.unlock();
    return temp;
}

template<class T>
bool BlockQueue<T>::push(const T &item){
    m_mutex.lock();
    if(m_size >= m_max_size){
        m_cond.broadcast();
        m_mutex.unlock();
        return flase;
    }
    m_back = (m_back + 1) % m_max_size;
    m_array[m_back] = item;

    m_size++;

    m_cond.broadcast();
    m_mutex.unlock();
    
    return true;
}

template<class T>
bool BlockQueue<T>::pop(const T &item){
    m_mutex.lock();
    while (m_size <= 0){
        
        if (!m_cond.wait(m_mutex.get())){
            m_mutex.unlock();
            return false;
        }
    }

    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];

    m_size--;

    m_mutex.unlock();
    return true;
}

template<class T>
bool BlockQueue<T>::pop(const T &item, int ms_timeout){
    struct timespec t = {0, 0};
    struct timeval now = {0, 0};

    gettimeofday(&now, nullptr);

    m_mutex.lock();

    if (m_size <= 0){
        t.tv_sec = now.tv_sec + ms_timeout / 1000;
        t.tv_nsec = (ms_timeout % 1000) * 1000;
        if (!m_cond.timewait(m_mutex.get(), t)){
            m_mutex.unlock();
            return false;
        }
    }

    if (m_size <= 0){
        m_mutex.unlock();
        return false;
    }

    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    
    m_size--;

    m_mutex.unlock();
    return true;
}