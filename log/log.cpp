#include "log.h"
#include <cstring>
#include <sys/time.h>

Log *Log::get_instance(){
    static Log instance;
    return &instance;
}

void *Log::flush_log_thread(void* args){
    Log::get_instance()->async_write_log();
}

bool Log::init(const char *file_name, int close_log, int log_buf_size /*= 8192*/, int split_lines /*= 5000000*/, int max_queue_size/* = 0 */){
    if(max_queue_size >= 1){
        m_is_async = true;
        m_log_queue = new BlockQueue<std::string>(max_queue_size);
        pthread_t tid;
        pthread_create(&tid, nullptr, flush_log_thread, nullptr);
    }

    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines = split_lines;

    time_t t = time(nullptr);
    tm *sys_tm = localtime(&t);
    tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    if(p == nullptr){
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else{
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a");

    if(m_fp != nullptr){
        return false;
    }

    return true;
}

void Log::write_log(int level, const char *format, ...){
    timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level){
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[erro]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;
    }

    m_mutex.lock();
    m_count++;

    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0){
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if(m_today != my_tm.tm_mday){
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else{
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }

        m_fp = fopen(new_log, "a");
        delete[] new_log;
    }

    m_mutex.unlock();

    va_list valst;
    va_start(valst, format);

    //通过log_str保存真正要写的日志数据
    std::string log_str;

    m_mutex.lock();

    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    //增加判断同步写入还是异步写入
    //异步
    if((m_is_async) && (!m_log_queue->full())){
        m_log_queue->push(log_str);
    }
    //同步
    else{
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    va_end(valst);
}

void Log::flush(){
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}

Log::Log():m_count(0), m_is_async(false){

}

Log::~Log(){
    if(m_fp != nullptr){
        fclose(m_fp);
    }
    if(m_log_queue != nullptr){
        delete[] m_log_queue;
    }
    if(m_buf != nullptr){
        delete[] m_buf;
    }
}

void *Log::async_write_log(){
    std::string single_log;
    //从阻塞队列中取出一个日志string，写入文件
    while (m_log_queue->pop(single_log))
    {
        m_mutex.lock();
        fputs(single_log.c_str(), m_fp);
        m_mutex.unlock();
    }
}