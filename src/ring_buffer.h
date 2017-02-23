#ifndef R_BUF_H
#define R_BUF_H

#include "alphy.h"

typedef struct ring_buf_t
{
    size_t      r;
    size_t      w;

    uchar_8     *buf;
    size_t      buf_size;

    mempool_t   *mempool;
} ring_buf_t;

// ring buffer 可以从mempool中分配,也可通过一段申请好的内存进行创建
// 通过mempool分配的ring buffer可以自增
ring_buf_t *rbuf_init(size_t size, mempool_t *mempool, uchar_8 *raw_buf);
void* rbuf_fini(ring_buf_t *rbuf);

size_t rbuf_put(ring_buf_t *rbuf, const void *buf, size_t len);
size_t rbuf_get(ring_buf_t *rbuf, void *buf, size_t len);
#endif
