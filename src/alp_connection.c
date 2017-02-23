#include <string.h>
#include <stdlib.h>
#include <time.h>

// 需要用红黑树代替
#include <map>

#include "alp_connection.h"

// 需要移动到配置中
const int TIMEOUT = 6000;

typedef std::map<int, connection_t *>::iterator ITER;
static std::map<int, connection_t *> live_connections;

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

