#include "pthread.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

#include "ring_buffer.h"

const int R_BUF_SIZE = 201;
const int TEST_TIMES = 2000;
const int IN_BUF_SIZE = 50;
const int OUT_BUF_SIZE = 50;
const char s[] = "";

void *push_data(void *rbuf)
{
    printf("enter push thread\n");
    unsigned int i,len, n;
    char in_buf[IN_BUF_SIZE];
    for (i = 0; i < TEST_TIMES; i++)
    {
        memset(in_buf, 0, IN_BUF_SIZE);
        /*sprintf(in_buf, "%s_%d\n", s, i);*/
        sprintf(in_buf, "%d\n", i);
        len = 0;
        // 阻塞的写入所有in_buf中数据
        while(len < strlen(in_buf))
        {
            n = rbuf_put((ring_buf_t *)rbuf
                    , in_buf + len
                    , strlen(in_buf) - len);
            len += n;
            /*printf("write <size:%d> data\n", n);*/
        }
    }
    return NULL;
}

void *pop_data(void *rbuf)
{
    printf("enter pop thread\n");
    unsigned int len;
    char out_buf[OUT_BUF_SIZE];
    while (1)
    {
        len = 0;
        memset(out_buf, 0, OUT_BUF_SIZE);
        len = rbuf_get((ring_buf_t *)rbuf
                , out_buf
                , sizeof(out_buf));
        /*printf("read <size:%d> data\n", len);*/
        if (len == 0) continue;
        printf("%.*s", len, out_buf);  
    }
}

int main()
{
    char *buf = (char *)malloc(R_BUF_SIZE);
    ring_buf_t *rbuf = rbuf_init(R_BUF_SIZE, buf);
    if (!buf) return -1;
    printf("create rbuf success\n");

    pthread_t thread_1;
    pthread_t thread_2;

    int ret = pthread_create(&thread_1, NULL, push_data, rbuf);
    if (ret != 0) {
        printf("create thread fail\n");
        return -1;
    }

    ret = pthread_create(&thread_2, NULL, pop_data, rbuf);
    if (ret != 0) {
        printf("create thread fail\n");
        return -1;
    }
    
    pthread_join(thread_1, NULL);
    pthread_join(thread_2, NULL);

    buf = (char *)rbuf_fini(rbuf);
    if (buf) free(buf);
}
