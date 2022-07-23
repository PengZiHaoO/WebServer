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

    if(one_shoot){
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

//外部调用的初始换成员函数
void HTTPConnection::init(int sockfd, const sockaddr_in& addr, char *root, int trig_mode,int close_log,std::string user, std::string passwd, std::string sqlname){
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_trig_mode);

    m_user_count++;

    doc_root = root;
    m_trig_mode = trig_mode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

//关闭socket连接
void HTTPConnection::close_conn(bool real_close/*= true*/){
    if(real_close && (m_sockfd != -1)){
        printf("close %d\n", m_sockfd);
        rmfd(m_epollfd, m_sockfd);

        m_sockfd = -1;
        m_user_count--;
    }
}

//
void HTTPConnection::process(){
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST){
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if(!write_ret){
        //process fail 关闭连接
        close_conn();
    }
    //重置oneshot
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trig_mode);
}

bool HTTPConnection::write(){
    if(bytes_to_send == 0){
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);
        init();
        return true;
    }

    int temp = 0;
    while (1){
         temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trig_mode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    
}

sockaddr_in* HTTPConnection::get_address(){
    return &m_address;
}

void HTTPConnection::initmysql_result(ConnectionPool *connPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    ConnectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        users[temp1] = temp2;
    }
}

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
        LOG_INFO("%s", text);
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
        LOG_INFO("ohhh!unkonw headers: %s", headers);
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


char* HTTPConnection::get_line(){
    return (m_read_buf + m_start_line);
}


HTTPConnection::HTTP_CODE HTTPConnection::do_request(){
    strcpy(m_real_file, doc_root);

    int len = strlen(doc_root);

    const char* p = strrchr(m_url, '/');

    //cgi判读method
    if(cgi == 1 && (*(p + 1) == '2') || *(p + 1) == '3'){
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);

        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILE_NAME_LEN - len -1);

        free(m_url_real);

        //提取usr name passwd
        //usr name passwd存在m_string中,"&"分隔
        char usr_name[100];
        char passwd[100];

        int i;
        for(i = 5; m_string[i] != '&'; ++i){
            usr_name[i - 5] = m_string[i];
        }
        usr_name[i - 5] = '\0';

        int j;
        for(i = i + 10; m_string[i] != '\0'; ++i, ++j){
            passwd[j] = m_string[i];
        }
        passwd[j] = '\0';

        if(*(p + 1) == '3'){
            //'3' 未注册
            //首先判断db中是否有重名
            //若无则注册
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, usr_name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, passwd);
            strcat(sql_insert, "')");

            //usr_name未重复
            if(users.find(usr_name) == users.end()){
                mutex.lock();
                int res = mysql_query(m_mysql, sql_insert);
                users.insert(std::pair<std::string, std::string>(usr_name, passwd));
                mutex.unlock();

                if(!res){
                    strcpy(m_url, "/log.html");
                }
                else{
                    strcpy(m_url, "/registerError.html");
                }
            }
            //重复
            else{
                strcpy(m_url, "/registerError.html");
            }
        }
        //登录
        //查询表判断数据
        else if(*(p + 1) == '2'){
            if (users.find(usr_name) != users.end() && users[usr_name] == passwd)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }
    if(*(p + 1) == '0'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if(*(p + 1) == '1'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if(*(p + 1) == '5'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if(*(p + 1) == '6'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if(*(p + 1) == '7'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/fans.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    free(m_url_real);
    }
    else{
        strncpy(m_real_file + len, m_url, FILE_NAME_LEN - len - 1);
    }

    if(stat(m_real_file, &m_file_stat) < 0){
        return NO_RESOURCE;
    }

    if(!(m_file_stat.st_mode & S_IROTH)){
        return BAD_REQUEST;
    }

    if(S_ISDIR(m_file_stat.st_mode)){
        return BAD_REQUEST;
    }

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    return FILE_REQUEST;
}

//将数据映射到内核提高访问速度
void HTTPConnection::unmap(){
    if(m_file_address){
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = nullptr;
    }
}

//
bool HTTPConnection::add_response(const char *format, ...){
    if(m_write_idx >= WRITE_BUFFER_SIZE){
        return false;
    }

    va_list arg_list;
    va_start(arg_list, format);

    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - m_write_idx - 1, format, arg_list);
    if(len >= (WRITE_BUFFER_SIZE - m_write_idx - 1)){
        va_end(arg_list);

        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("request:%s, m_write_buf");

    return true;
}

//添加状态行信息
bool HTTPConnection::add_status_line(int status, const char *title){
    return add_response("%s %d %s \r\n", "HTTP/1.1", status, title);
}

//添加头部行信息
bool HTTPConnection::add_headers(int content_len){
    return (add_content_length(content_len) && add_linger() && add_blank_line());
}

//添加消息体内容长度
bool HTTPConnection::add_content_length(int content_len){
    return add_response("Content-Length:%d\r\n", content_len);
}

//添加消息体内容格式
bool HTTPConnection::add_content_type(){
    return add_response("Content-Type:%s\r\n", "text/html");
}

//添加是否保持连接
bool HTTPConnection::add_linger(){
    return add_response("Connection:%s\r\n",((m_linger == true)?"keep-alive": "close"));
}

//添加空行
bool HTTPConnection::add_blank_line(){
    return add_response("%s", "\r\n");
}

//添加消息体实质的消息内容
bool HTTPConnection::add_content(const char *content){
    return add_response("%s", content);
}





