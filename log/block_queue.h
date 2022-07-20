#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <cstdlib>
#include <pthread.h>
#include <sys/time.h>

#include "../locker/locker.hpp"

template<class T>
class BlockQueue{
public:
    BlockQueue(int max_size = 1000);
    ~BlockQueue();

    void clear();
    bool full();
    bool empty();
    bool front(T &value);
    bool back(T &value);
    int size();
    int max_size();

    bool push(const T &item);
    bool pop(const T&item);
    bool pop(const T&item, int ms_timeout);

private:
    Mutex m_mutex;
    Condition m_cond;

    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
};

#endif