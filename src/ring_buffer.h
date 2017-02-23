#ifndef R_BUF_H
#define R_BUF_H

typedef struct ring_buf_t
{
    unsigned int r;
    unsigned int w;

    char*  buf;
    unsigned int buf_size;
} ring_buf_t;

// ring buffer不负责内存的管理
ring_buf_t *rbuf_init(unsigned int size, char *buf);
void* rbuf_fini(ring_buf_t *rbuf);

unsigned int rbuf_put(ring_buf_t *rbuf, const void *buf, unsigned int len);
unsigned int rbuf_get(ring_buf_t *rbuf, void *buf, unsigned int len);
#endif
