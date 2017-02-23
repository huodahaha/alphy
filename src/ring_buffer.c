#include "assert.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "ring_buffer.h"

#define MIN(a,b) a>b?b:a

// 实际可写空间为buf_size-1
// r == w 时，缓冲区空， r+1%size == w时，缓冲区满
ring_buf_t *rbuf_init(unsigned int size, char *buf)
{
    ring_buf_t *rb = (ring_buf_t *)malloc(sizeof(ring_buf_t));
    if (!rb) return rb;

    rb->r = 0;
    rb->w = 0;
    rb->buf = buf;
    rb->buf_size = size;
    return rb;
}

void* rbuf_fini(ring_buf_t *rbuf)
{
    if (!rbuf) return NULL;
    void *raw_buf = rbuf->buf;
    free(rbuf);

    return raw_buf;
}

unsigned int rbuf_put(ring_buf_t *rbuf, const void *buf, unsigned int len)
{
    if (!rbuf) return 0;
    if (!buf) return 0;
 
    unsigned int r = rbuf->r;
    unsigned int w = rbuf->w;
    unsigned int space = w > r? (w-r-1):(w+rbuf->buf_size-r-1);
    if (space == 0) return 0;

    unsigned int size = MIN(len, space);
    unsigned int cpy_size_1 = MIN(size, rbuf->buf_size - rbuf->r);
    memcpy(rbuf->buf + rbuf->r, buf, cpy_size_1);

    if (cpy_size_1 != size)
    {
        int cpy_size_2 = size - cpy_size_1;
        memcpy(rbuf->buf, (char*)buf + cpy_size_1, cpy_size_2);
    }

    // 写入数据之后再写指针，避免多线程下的问题
    rbuf->r = (size + rbuf->r)%(rbuf->buf_size);
    
    return size;
}

unsigned int rbuf_get(ring_buf_t *rbuf, void *buf, unsigned int len)
{
    if (!rbuf) return 0;
    if (!buf) return 0; 

    unsigned int r = rbuf->r;
    unsigned int w = rbuf->w;
    unsigned int buf_size = r >= w? (r-w):(r+rbuf->buf_size-w);
    if (buf_size == 0) return 0;
    unsigned int size = MIN(len, buf_size);

    unsigned int cpy_size_1 = MIN(size, rbuf->buf_size - rbuf->w);
    memcpy(buf, rbuf->buf + rbuf->w, cpy_size_1);

    if (cpy_size_1 != size)
    {
        int cpy_size_2 = size - cpy_size_1;
        memcpy((char*)buf + cpy_size_1, rbuf->buf, cpy_size_2);
    }

    // 写入数据之后再写指针，避免多线程下的问题
    rbuf->w = (size + rbuf->w)%(rbuf->buf_size);
    return size;
}
