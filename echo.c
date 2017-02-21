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

// todo 需要修改为C实现的红黑树
#include <map>

#define PORT 6666
#define LISTENQ 20
#define EPOLLEVENTS 1024
#define MAXNCONNS 65536

// 前置定义
struct connection_t;

// poller 
// 为了防止poller循环引用，明确poller依赖connection
// 向poller注册事件，修改事件应该是一个对外的接口，connection可以调用poller的方法
static int poller_add_event(const connection_t *conn);
static int poller_add_event(const int epfd, const int fd, const int op, void* cb_data);

static int poller_update_event(connection_t *conn);
static int poller_update_event(const int epfd, const int fd, const int op, void* cb_data);

static int poller_del_event(connection_t *conn);
static int poller_del_event(const int epfd, const int fd);

static void process_events(int epfd, epoll_event *events, int n, int listen_fd);

// todo handler中回调connection的cb执行具体逻辑
static void handle_accept(int epfd, int listen_fd);
static void handle_read(int epfd, epoll_event *event);
static void handle_close(int epfd, epoll_event *event);
static void handle_write(int epfd,epoll_event *event);

// connection相关
// todo: connection池应该是全局唯一的，所有connection的增删改差都由一个connection池对象决定
struct buffer_t;
struct connection_t;
struct memory_pool_t;

typedef int (*recv_handler)(connection_t* conn);

// 此处为一个定长的buffer，应该有mempool分配，一次分配之后，只有整个connection结束后消失
// 此外，有可能出现读写buffer满的问题，为解决这个现象，应该由mempool分配一个链表，形成一个可动态增长的块
struct buffer_t
{
    int     len;
    char    buf[1024];
};

struct memory_pool_t
{
    
};

// 被动连接
#define READ_NEED   0x01
#define WRITE_NEED  0x02
#define set_read(s) s|=READ_NEED
#define set_write(s) s|=WRITE_NEED
#define is_read(s) s&READ_NEED
#define is_write(s) s&WRITE_NEED
#define reset_read(s) s&=(~READ_NEED)
#define reset_write(s) s&=(~WRITE_NEED)

// 用作listen fd的callback data，和connection的第一个值都是fd
struct acceptor_cb_data_t
{
    int fd;
};

struct connection_t
{
    int             fd;
    int             epfd;
    
    memory_pool_t   *mempool;
    buffer_t        rcv_buf;
    buffer_t        send_buf;

    recv_handler    handler;    

    int             last_operate;   // 用于链接的超时回收
    
    // 由于connection不能向上调用poller层，所以将关注读写状态定义在connection中，有等待poller update
    int             rw_status;
};

// todo 应该转移到配置中 
const int TIMEOUT = 6000; 
typedef std::map<int, connection_t *>::iterator ITER;

// todo 此处应该实现一个管理链接的对象
std::map<int, connection_t *> live_connections;

connection_t *alloc_connection(const int epfd, const int fd);
void update_connection(connection_t *conn);
void clean_connection(connection_t *conn);
void clean_timeout_conns();

// 定义测试用回调
static int echo(connection_t* conn) 
{
    //todo 定义装饰器，调用handler时自动update
    update_connection(conn);

    buffer_t &rcv_buf = conn->rcv_buf;
    /*printf("echo %.*s\n", rcv_buf.len, rcv_buf.buf); */

    memcpy(conn->send_buf.buf, rcv_buf.buf, rcv_buf.len);
    conn->send_buf.len = conn->rcv_buf.len;
    conn->rcv_buf.len = 0;
    set_write(conn->rw_status);
    return 0;
}

// 测试用回调，清除写标志
/*static int clean_read_flag(connection_t* conn)*/
/*{*/
    /*set_read(conn->rw_status);*/
    /*return 0;*/
/*};*/

// socket 相关
static int tcp_listen(int port);
static void set_nonblock(int fd);

// internal functions
connection_t *alloc_connection(const int fd, const int epfd)
{
    connection_t *conn = (connection_t *)malloc(sizeof(connection_t));
    if (conn == NULL) return conn;
   
    conn->fd = fd;
    conn->epfd = epfd;
    // connection的处理回调为echo
    conn->handler = &echo;
    live_connections[fd] = conn;
    update_connection(conn);
    set_read(conn->rw_status);
    return conn;
}

void update_connection(connection_t *conn)
{
    int now = time((time_t*)NULL);
    live_connections[conn->fd]->last_operate = now; 
}

// 关于socket的释放等，由poller层处理
void clean_connection(connection_t *conn)
{
    ITER iter = live_connections.find(conn->fd);
    if (iter != live_connections.end()){
        // todo 回收内存
        live_connections.erase(iter);        
    }
    // 回收connection 结构体空间，todo 将失效connection返回给内存池
    free(conn); 
}

void clean_timeout_conns()
{ 
    int now = time((time_t*)NULL);
    for (ITER iter = live_connections.begin(); 
            iter != live_connections.end(); iter++)
    {
        if(iter->second->last_operate - now > TIMEOUT){
            // time out
            clean_connection(iter->second);
        }
    }
}

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
    int fd;
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


int main()
{
    int listen_fd;

    listen_fd=tcp_listen(PORT);
    epoll_loop(listen_fd);

    return 0;
}

