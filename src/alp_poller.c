#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <fcntl.h>

#include <time.h>

#include "alp_connection.h"


// todo 移动到配置中
#define LISTENQ 20
#define EPOLLEVENTS 2048

// poller 
// 向poller注册事件，修改事件应该是一个对外的接口，connection可以调用poller的方法
static int poller_add_event(connection_t *conn);
static int poller_add_event(const int epfd, const int fd, const int op, void* cb_data);

static int poller_update_event(connection_t *conn);
static int poller_update_event(const int epfd, const int fd, const int op, void* cb_data);

static int poller_del_event(connection_t *conn);
static int poller_del_event(const int epfd, const int fd);

static void process_events(int epfd, epoll_event *events, int n, int listen_fd);

// todo handler中回调connection的cb执行具体逻辑
static void handle_accept(int epfd, int listen_fd);
static void handle_read(epoll_event *event);
static void handle_close(epoll_event *event);
static void handle_write(epoll_event *event);

// socket 相关
static void set_nonblock(int fd);

int tcp_listen(int port)
{
    int listen_fd;
    struct sockaddr_in serv_addr;
    if((listen_fd=socket(AF_INET,SOCK_STREAM,0))<0)
    {
        fprintf(stderr,"socket error\n");
        exit(1);
    }

    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(port);
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);

    if(bind(listen_fd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
    {
        fprintf(stderr,"bind error\n");
        exit(1);
    }

    if(listen(listen_fd,LISTENQ)<0)
    {
        fprintf(stderr,"listen error");
        exit(1);
    }

    return listen_fd;
}

void set_nonblock(int fd)
{
    int flags;
    if((flags=fcntl(fd,F_GETFL,0))<0)
    {
        fprintf(stderr,"fcntl error");
        fprintf(stderr,"<%d %s>", errno, strerror(errno));
        exit(1);
    }

    if(fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)
    {
        fprintf(stderr,"set nonblock failed");
        exit(1);
    }
}

// 为异步IO增加一个关注事件
int poller_add_event(connection_t *conn)
{
    // todo
    int op = EPOLLET | EPOLLRDHUP;
    if (is_write(conn->rw_status)) op |= EPOLLOUT;
    if (is_read(conn->rw_status)) op |= EPOLLIN;

    return poller_add_event(conn->epfd, conn->fd, op, conn);
}

int poller_add_event(const int epfd, const int fd, const int op, void* cb_data)
{
    epoll_event ev;

    ev.events = op | EPOLLET;
    ev.data.ptr = cb_data;

    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    return 0;
}

// 修改socket的关注事件，修改为可读或可写
int poller_update_event(connection_t *conn)
{
    // todo
    int op = EPOLLET | EPOLLRDHUP;
    if (is_write(conn->rw_status)) op |= EPOLLOUT;
    if (is_read(conn->rw_status)) op |= EPOLLIN;

    return poller_update_event(conn->epfd, conn->fd, op, conn);
}

int poller_update_event(const int epfd, const int fd, const int op, void* cb_data)
{
    epoll_event ev;

    ev.events = op | EPOLLET;
    ev.data.ptr = cb_data;

    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    return 0;
}

int poller_del_event(connection_t *conn)
{
    return poller_del_event(conn->epfd, conn->fd);
}

int poller_del_event(const int epfd, const int fd)
{
    epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
    return 0;
}

// todo 临时做法
static void poller_clean_connection(connection_t* conn)
{
    printf("handle close <%d>\n", conn->fd);
    close(conn->fd);
    poller_del_event(conn);
    clean_connection(conn);
    return;
}

void handle_accept(int epfd, int listen_fd)
{
    int conn_fd;
    struct sockaddr_in cli_addr;
    socklen_t cli_len;

    conn_fd = accept(listen_fd,(struct sockaddr *)&cli_addr,&cli_len);
    if(conn_fd<0)
    {
        fprintf(stderr,"accpet client connection error");
        fprintf(stderr,"<%d %s>", errno, strerror(errno));
        return;
    }
    set_nonblock(conn_fd);

    printf("accept done! <%d>\n", conn_fd);

    connection_t *conn = alloc_connection(conn_fd, epfd);
    poller_add_event(conn);
}

void handle_read(epoll_event *event)
{
    int n;
    bool need_clean = false;

    connection_t *conn = (connection_t *)event->data.ptr;
    buffer_t &buf = conn->rcv_buf;
    int &buf_len = buf.len;
    int fd = conn->fd;

    while (1)
    {
        // 预留1byte，防止溢出
        n = read(fd, buf.buf, sizeof(buf.buf) - buf_len - 1);

        if (n > 0){
            buf_len += n;
            if (buf_len >= (int)sizeof(buf.buf) - 1)
            {
                printf("recv buffer overflow\n");
                break;
            }
            else continue;
        }
        else if (n == 0) {            
            need_clean = true;
            break;
        }
        else if ((n = -1)) {
            if (errno == EAGAIN){
                break;
            }
        
            printf("error while reading from peer, <%d, %s>\n", errno, strerror(errno));
            need_clean = true;
            break; 
        }
    }
 
    printf("rcv %.*s\n", buf.len, buf.buf); 

    if (need_clean)
        poller_clean_connection(conn);
    else { 
        // 回调处理 
        conn->handler(conn);
        poller_update_event((connection_t *)conn);
    }
}

void handle_close(epoll_event *event)
{
    connection_t *conn = (connection_t *)event->data.ptr;
    
    poller_clean_connection(conn);
    return;
}

void handle_write(epoll_event *event)
{
    int n;
    bool need_clean = false;

    connection_t *conn = (connection_t *)event->data.ptr;
    buffer_t &buf = conn->send_buf;
    int buf_len = buf.len;
    int fd = conn->fd;
 
    printf("write %.*s\n", buf.len, buf.buf); 

    while (1)
    {
        n = write(fd, buf.buf, buf_len);

        if (n > 0){
            // todo
            reset_write(conn->rw_status);
            break;
        }
        else if ((n <= -1)) {
            if (errno == EAGAIN){
                // todo: socket缓冲区已满
                break;
            }
        
            printf("error while writng, <%d, %s>\n", errno, strerror(errno));
            close(conn->fd);
            break; 
        }
    }

    // update
    if (need_clean)
        poller_clean_connection(conn);
    else { 
        poller_update_event((connection_t *)conn);
    }
}

void process_events(int epfd, epoll_event *events, int n, int listen_fd)
{
    for(int i=0; i<n; i++)
    {
        void* conn = events[i].data.ptr; 

        if(conn == NULL && (events[i].events&EPOLLIN))
        {
            handle_accept(epfd,listen_fd);
            continue;
        }

        if(events[i].events&EPOLLRDHUP)
        {
            // 解决大量close_wait
            handle_close(&events[i]);
            continue;
        }

        if(events[i].events&EPOLLIN)
        {
            handle_read(&events[i]);
        }

        if(events[i].events&EPOLLOUT)
        {
            handle_write(&events[i]);
        }
    }
}

void epoll_loop(int listen_fd)
{
    int epfd,nready;
    epoll_event *events;

    events=(epoll_event *)malloc(sizeof(epoll_event)*EPOLLEVENTS);
    set_nonblock(listen_fd);
    epfd=epoll_create(1);

    if(poller_add_event(epfd,listen_fd,EPOLLIN, NULL)<0)
    {
        fprintf(stderr,"add listenfd to epoll failed");
        exit(1);
    }
    for(;;)
    {
        nready=epoll_wait(epfd,events,EPOLLEVENTS,-1);
        process_events(epfd,events,nready,listen_fd);
        clean_timeout_conns();
    }

    free(events);
    close(epfd);
}
