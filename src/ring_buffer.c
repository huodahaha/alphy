#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "ring_buffer.h"
#include "mempool.h"

static int_32 _rbuf_resize(ring_buf_t *rbuf)
{
    size_t size = rbuf->buf_size;

    uchar_8 *new_raw_buf = (uchar_8 *)pool_alloc(rbuf->mempool, size * 2);
    if (!new_raw_buf) return ALPHY_DECLINED;

    memcpy(new_raw_buf, rbuf->buf, size);
    
#ifdef DEBUG_RINGBUFFER
    printf("ring buffer has been resized <old:%d\tnew:%d>\n", size, size * 2);
#endif

    rbuf->buf_size = size * 2;
    pool_free(rbuf->mempool, rbuf->buf);
    rbuf->buf = new_raw_buf;
    return ALPHY_OK;
}

ring_buf_t *rbuf_init(size_t size, mempool_t *mempool, uchar_8 *raw_buf)
{
    ring_buf_t *rb = NULL;
    if (mempool) {
        rb = (ring_buf_t *)pool_alloc(mempool, sizeof(ring_buf_t));
        rb->buf = (uchar_8 *)pool_alloc(mempool, size);
    }
    else {
        if (!raw_buf) return NULL;
        rb = (ring_buf_t *)naive_alloc(sizeof(ring_buf_t));
        rb->buf = raw_buf;
    }

    if (!rb) return rb; 

    rb->r = 0;
    rb->w = 0;
    rb->buf_size = size;
    rb->mempool = mempool;
    return rb;
}

void* rbuf_fini(ring_buf_t *rbuf)
{
    if (!rbuf) return NULL;
    void *raw_buf = rbuf->buf;

    if (rbuf->mempool){
        mempool_t *mempool = rbuf->mempool;
        pool_free(mempool, raw_buf);
        // 不需要free ring_buffer_t,内存池会自动进行清理
        raw_buf = NULL;
    }
    else {
        naive_free(rbuf);
    }

    return raw_buf;
}

// 实际可写空间为buf_size-1
// r == w 时，缓冲区空， r+1%size == w时，缓冲区满
size_t rbuf_put(ring_buf_t *rbuf, const void *buf, size_t len)
{
    if (!rbuf) return 0;
    if (!buf) return 0;
 
    size_t space, write_size, cpy_size_1, cpy_size_2;
    size_t r = rbuf->r;
    size_t w = rbuf->w;

    bool is_resizing = true;
    write_size = len;
    do {
        space = w > r? (w-r-1):(w+rbuf->buf_size-r-1);

        if (!rbuf->mempool){
            is_resizing = false;
        }
        else{
            if ((space >= len) ||
                (_rbuf_resize(rbuf) != ALPHY_OK)){
                is_resizing = false;
            }
        }

        if (!is_resizing){
            if (space == 0) return 0;
            write_size = min(len, space);
            break;
        } 
    } while (space < len);

    cpy_size_1 = min(write_size, rbuf->buf_size - rbuf->r);
    memcpy(rbuf->buf + rbuf->r, buf, cpy_size_1);

    if (cpy_size_1 != write_size){
        cpy_size_2 = write_size - cpy_size_1;
        memcpy(rbuf->buf, (uchar_8 *)buf + cpy_size_1, cpy_size_2);
    }

    // 写入数据之后再写指针，避免多线程下的问题
    rbuf->r = (write_size + rbuf->r)%(rbuf->buf_size);
    
    return write_size;
}

size_t rbuf_get(ring_buf_t *rbuf, void *buf, size_t len)
{
    if (!rbuf) return 0;
    if (!buf) return 0; 

    size_t get_size, cpy_size_1, cpy_size_2;
    size_t r = rbuf->r;
    size_t w = rbuf->w;
    size_t buf_size = r >= w? (r-w):(r+rbuf->buf_size-w);
    if (buf_size == 0) return 0;
    
    get_size = min(len, buf_size);
    cpy_size_1 = min(get_size, rbuf->buf_size - rbuf->w);
    memcpy(buf, rbuf->buf + rbuf->w, cpy_size_1);

    if (cpy_size_1 != get_size){
        cpy_size_2 = get_size - cpy_size_1;
        memcpy((uchar_8 *)buf + cpy_size_1, rbuf->buf, cpy_size_2);
    }

    // 写入数据之后再写指针，避免多线程下的问题
    rbuf->w = (get_size + rbuf->w)%(rbuf->buf_size);
    return get_size;
}
