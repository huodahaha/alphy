#ifndef ALP_CONNECTION_H
#define ALP_CONNECTION_H

// connection相关
// todo: connection池应该是全局唯一的，所有connection的增删改差都由一个connection池对象决定
struct buffer_t;
struct connection_t;
struct mempool_t;

typedef int (*recv_handler)(connection_t* conn);

// 此处为一个定长的buffer，应该有mempool分配，一次分配之后，只有整个connection结束后消失
// 此外，有可能出现读写buffer满的问题，为解决这个现象，应该由mempool分配一个链表，形成一个可动态增长的块
struct buffer_t
{
    int     len;
    char    buf[1024];
};

// 被动连接
#define ALP_READ_NEED   0x01
#define ALP_WRITE_NEED  0x02

#define set_read(s) s|=ALP_READ_NEED
#define set_write(s) s|=ALP_WRITE_NEED

#define is_read(s) s&ALP_READ_NEED
#define is_write(s) s&ALP_WRITE_NEED

#define reset_read(s) s&=(~ALP_READ_NEED)
#define reset_write(s) s&=(~ALP_WRITE_NEED)

// 用作listen fd的callback data，和connection的第一个值都是fd
struct acceptor_cb_data_t
{
    int fd;
};

struct connection_t
{
    int             fd;
    int             epfd;
    
    mempool_t       *mempool;
    buffer_t        rcv_buf;
    buffer_t        send_buf;

    recv_handler    handler;    

    int             last_operate;   // 用于链接的超时回收
    
    // connection 不主动注册事件，事件管理由poller update 
    int             rw_status;
};

//connection_t *alloc_connection(const int epfd, const int fd);
void update_connection(connection_t *conn);
void clean_connection(connection_t *conn);
void clean_timeout_conns();

#endif
