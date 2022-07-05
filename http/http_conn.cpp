#include "http_conn.h"

const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file form this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the request file.\n";

Mutex mutex;
std::map<std::string, std::string> users;

//epoll io多路复用的一些实现

//对文件描述符设置为非阻塞
int set_nonblocking(int fd){
    int past_option = fcntl(fd, F_GETFL);
    int now_nonblocking_option = past_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, now_nonblocking_option);
    return past_option;
}

//向内核事件表注册事件, 选择ET模式, 选择开始EPOLLONESHOOT
void addfd(int epfd, int fd, bool one_shoot, int trig_mode){
    epoll_event event;
    event.data.fd = fd;

    if(1 == trig_mode){
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else{
        event.events = EPOLLIN | EPOLLHUP;
    }

    if(1 == one_shoot){
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

//从事件表删除事件
void rmfd(int epfd, int fd){
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epfd, int fd, int ev, int trig_mode){
    epoll_event event;
    event.data.fd = fd;

    if(1 == trig_mode){
        event.events = ev | EPOLLIN | EPOLLHUP | EPOLLONESHOT;
    }
    else{   
        event.events = ev | EPOLLIN | EPOLLHUP;
    }

    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);
}

int HTTPConnection::m_epollfd = -1;
int HTTPConnection::m_user_count = 0;

//私有init函数 初始化一些成员变量
void HTTPConnection::init(){
    m_mysql = NULL;
    bytes_to_send = 0;
    bytes_to_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILE_NAME_LEN);
}

//进程读取数据
HTTPConnection::HTTP_CODE HTTPConnection::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK)){
        text = get_line();

        m_start_line = m_checked_idx;
        //LOG_INFO("%s", text);
        switch (m_check_state){
            case CHECK_STATE_REQUESTLINE:
                ret = parse_request_line(text);
                
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }

                break;
            case CHECK_STATE_HEADER:
                ret = parse_headers(text);

                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST){
                    return do_request();
                }
                break;
            case CHECK_STATE_CONTENT:
                ret = parse_content(text);

                if(ret == GET_REQUEST){
                    return do_request();
                }

                line_status = LINE_OPEN;
                break;
            default:
                return INTERNAL_ERROR;
        }
    }
}