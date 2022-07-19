#include "timer.h"
#include "../http/http_conn.h"

//定时器容器构造函数
TimerContainer::TimerContainer(): m_head(nullptr), m_tail(nullptr){

}

//析构函数
TimerContainer::~TimerContainer(){
    UtilityTimer *temp = m_head;

    while (m_head != nullptr){
        m_head = temp->next;
        delete temp;
        temp = m_head->next;
    }
    
}

//按expire升序插入节点
void TimerContainer::add_timer_node(UtilityTimer *timer_node, UtilityTimer *head){
    UtilityTimer *curr = head;
    UtilityTimer *temp = curr->next;
    //从head开始遍历
    //直到timer_node->expire < temp->expire
    while (temp != nullptr){
        if(timer_node->expire < temp->expire){
            curr->next = timer_node;
            timer_node->next = temp;
            temp->prev = timer_node;
            timer_node->prev = curr;
            //跳出循环
            break;
        }
        curr = curr->next;
        temp = curr->next;
    }
    
    //出循环时temp为空指针则timer_node需要插在双向链表最后
    if(temp == nullptr){
        curr->next = timer_node;
        timer_node->prev = curr;
        timer_node->next = nullptr;
        m_tail = timer_node;
    }
}

//插入节点
void TimerContainer::add_timer_node(UtilityTimer *timer_node){
    if(timer_node == nullptr){
        return;
    }
    
    if(m_head == nullptr){
        m_head = m_tail = timer_node;
        return;
    }

    if(timer_node->expire < m_head->expire){
        timer_node->next = m_head; 
        m_head->prev = timer_node;
        m_head = timer_node;
        return;
    }

    add_timer_node(timer_node, m_head);
}


void TimerContainer::adjust_timer_node(UtilityTimer *timer_node){
    if(timer_node == nullptr){
        return;
    }

    //tiemr_node为尾节点或者超时值小于下一个节点的超时值
    //不需调整
    UtilityTimer *temp = timer_node->next;
    if((temp == nullptr) || (timer_node->expire < temp->expire)){
        return;
    }
    //timer_node为头节点
    //头节点后移
    //timer_node重新插入链表
    if(timer_node == m_head){
        m_head = m_head->next;
        m_head->prev = nullptr;
        timer_node->next = nullptr;
        
        add_timer_node(timer_node, m_head);
    }
    //timer_node为中间节点将其取出
    //重新插入链表
    else{
        timer_node->prev->next = timer_node->next;
        timer_node->next->prev = timer_node->prev;

        add_timer_node(timer_node, timer_node->next);
    }
}

//删除定时器节点
//重新连接链表
//delete已分配的节点内存即可
void TimerContainer::delete_timer_node(UtilityTimer *timer_node){
    if(timer_node == nullptr){
        return;
    }

    if((timer_node == m_head) && (timer_node == m_tail)){
        delete timer_node;
        m_head = nullptr;
        m_tail = nullptr;
        return;
    }

    if(timer_node == m_head){
        m_head = timer_node->next;
        m_head->prev = nullptr;
        delete timer_node;
        return;
    }

    if(timer_node == m_tail){
        m_tail = timer_node->prev;
        m_tail->next = nullptr;
        delete timer_node;
        return;
    }

    timer_node->prev->next = timer_node->next;
    timer_node->next->prev = timer_node->prev;
    delete timer_node;
}

//定时任务处理函数
void TimerContainer::timer_tick(){
    if(m_head == nullptr){
        return;
    }

    time_t curr_time = time(nullptr);
    UtilityTimer *temp = m_head;
    while (temp != nullptr){
        if(curr_time < temp->expire){
            break;
        }

        temp->timer_event(temp->user_data);
        m_head = temp->next;
        
        if(m_head != nullptr){
            m_head->prev = nullptr;
        }
        delete temp;
        temp = m_head;
    }
}

//定时事件
void timer_event(ClientData *user_data){
    epoll_ctl(Utility::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    HTTPConnection::m_user_count--;
}

//初始化timeslot
void Utility::init(int timeslot){
    m_timeslot = timeslot;
}


int Utility::setnonblocking(int fd){
    int past_option = fcntl(fd, F_GETFL);
    int now_nonblocking_option = past_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, now_nonblocking_option);
    return past_option;
}

void Utility::addfd(int epfd, int fd, bool one_shoot, int trig_mode){
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
    setnonblocking(fd);
}

//信号处理函数 
//将信号通过管道发送至用户状态下的进程
void Utility::sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void Utility::add_sig(int sig, void(handler)(int), bool restart/*= true*/){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;

    if(restart){
        sa.sa_flags |= SA_RESTART;
    }

    sigfillset(&sa.sa_mask);

    assert(sigaction(sig, &sa, nullptr) != -1);
}

void Utility::timer_handler(){
    m_timer_container.timer_tick();
    alarm(m_timeslot);
}

void Utility::show_error(int connfd, const char *info){
    send(connfd, info, strlen(info), 0);
}

int  Utility::u_epollfd = 0;
int *Utility::u_pipefd = 0;

