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

//处理读取需要读取的数据
HTTPConnection::HTTP_CODE HTTPConnection::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK)){
        text = get_line();

        m_start_line = m_checked_idx;
        //LOG_INFO("%s", text);
        switch (m_check_state){
            case CHECK_STATE_REQUESTLINE:{
                ret = parse_request_line(text);
                
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }

                break;
            }
            case CHECK_STATE_HEADER:{
                ret = parse_headers(text);

                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST){
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:{
                ret = parse_content(text);

                if(ret == GET_REQUEST){
                    return do_request();
                }

                line_status = LINE_OPEN;
                break;
            }
            default:{
                return INTERNAL_ERROR;
            }
        }
    }

    return NO_REQUEST;
}

//处理需要写的数据
bool HTTPConnection::process_write(HTTP_CODE ret){
    switch (ret){
        case INTERNAL_ERROR:{
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));

            if(!add_content(error_500_form)){
                return false;
            }

            break;
        }
        case BAD_REQUEST:{
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));

            if(!add_content(error_400_form)){
                return false;
            }

            break;
        }
        case FORBIDDEN_REQUEST:{
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));

            if(!add_content(error_404_form)){
                return false;
            }

            break;
        }
        case FILE_REQUEST:{
            add_status_line(404, error_400_title);

            if(m_file_stat.st_size != 0){
                add_headers(m_file_stat.st_size);

                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;

                m_iv_count = 2;

                bytes_to_send = m_checked_idx + m_file_stat.st_size;

                return true;
            }
            else{
                const char* ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));

                if(!add_content(error_404_form)){
                    return false;
                }
            }
            break;
        }
        default:{
            return false;
        }
    }

    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;

    return true;
}

//一次性读取用户数据
bool HTTPConnection::read_once(){
    if(m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }

    int bytes_read = 0;

    //LT模式读取数据
    if(0 == m_trig_mode){
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        
        if(bytes_read <= 0){
            return false;
        }

        m_read_idx += bytes_read;

        return true;
    }
    //ET模式读取数据
    else{
        while(true){
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);

            if(-1 == bytes_read){
                if(errno == EAGAIN || errno == EWOULDBLOCK)

                return false;
            }
            else if(0 == bytes_read){
                return false;
            }

            m_read_idx += bytes_read;
        }
        return true;
    }

    return false;
}

//从状态机
HTTPConnection::LINE_STATUS HTTPConnection::parse_line(){
    char temp = 0;

    for(; m_checked_idx < m_read_idx; ++m_checked_idx){
        temp = m_read_buf[m_checked_idx];

        if(temp == '\r'){
            if((m_checked_idx + 1) == m_read_idx){
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_idx + 1] == '\n'){
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';

                return LINE_OK;
            }
            
            return LINE_BAD;
        }
        else if(temp == '\n'){
            if((m_read_idx > 1) && (m_read_buf[m_checked_idx - 1] == '\r')){
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';

                return LINE_OK;
            }

            return LINE_BAD;
        }
    }

    return LINE_OPEN;
}


//主状态机
//解析请求行 获取url 请求方法 和 http协议版本
HTTPConnection::HTTP_CODE HTTPConnection::parse_request_line(char* text){
    //获取url
    m_url = strpbrk(text, " \t");
    if(!m_url){
        return BAD_REQUEST;
    }

    *(m_url++) = '\0';
    //获取请求方法
    char* method = text;
    if(strcasecmp(method, "GET")){
        m_method = GET;
    }
    else if(strcasecmp(method, "POST")){
        m_method = POST;
        cgi = 1;
    }
    else{
        return BAD_REQUEST;
    }

    //获取http协议版本
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");

    if(!m_version){
        return BAD_REQUEST;
    }

    *(m_version++) = '\0';
    m_version += strspn(m_version, " \t");

    if(strcasecmp(m_version, "HTTP/1.1") != 0){
        return BAD_REQUEST;
    }

    if(strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    //当url为/时，显示判断界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析HTTP请求头部行
HTTPConnection::HTTP_CODE HTTPConnection::parse_headers(char* headers){
    if(headers[0] == '\0'){
        if(m_content_length != 0){
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(headers, "Connection:", 11) == 0){
        headers += 11;
        headers += strspn(headers, " \t");

        if(strcasecmp(headers, "keep-alive") == 0){
            m_linger = true;
        }
    }
    else if(strncasecmp(headers, "Content-length:", 15) == 0){
        headers += 15;
        headers += strspn(headers, " \t");

        m_content_length = atol(headers);
    }
    else if(strncasecmp(headers, "Host:", 5) == 0){
        headers += 5;
        headers += strspn(headers, " \t");

        m_host = headers;
    }
    else{
        //LOG_INFO("ohhh!unkonw headers: %s", headers);
    }

    return NO_REQUEST;
}

//解析HTTP请求内容即判断HTTP请求是否完整
HTTPConnection::HTTP_CODE HTTPConnection::parse_content(char* text){
    if(m_read_idx >= (m_content_length + m_checked_idx)){
        text[m_content_length] = '\0';

        m_string = text;

        return GET_REQUEST;
    }

    return NO_REQUEST;
}

