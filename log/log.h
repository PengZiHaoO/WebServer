#ifndef LOG_H
#define LOG_H

#include <cstdio>
#include <string>
#include <stdarg.h>
#include <pthread.h>

#include "block_queue.h"

class Log{
public:
    //单例模式 懒汉模式实例化
    static Log *get_instance();
    
    static void *flush_log_thread(void* args);

    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    void write_log(int level, const char *format, ...);

    void flush();

private:

    Log();
    virtual ~Log();
    void *async_write_log();

private:

    char dir_name[128];
    char log_name[128];
    int m_split_lines;
    int m_log_buf_size;
    long long m_count;
    int m_today;
    FILE *m_fp;
    char *m_buf; 
    BlockQueue<std::string> *m_log_queue;
    bool m_is_async;
    Mutex m_mutex;
    int m_close_log;

};


#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif