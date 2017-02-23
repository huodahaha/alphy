#include "pthread.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "assert.h"

#include "ring_buffer.h"

#define TEST_STR_LEN 10240
#define RECV_BUF_LEN 10240
#define SINGLE_MAX 128
#define MEMPOOL_SIZE 10240
#define RINGBUFFER_SIZE 512

char input_str[TEST_STR_LEN];
char output_str[RECV_BUF_LEN];

size_t get_random_size(size_t max)
{
    return rand()%max + 1;
}

char *rand_str(char *str,const size_t len)
{
    size_t i;
    for(i=0;i<len;++i)
        str[i]='A'+rand()%26;
    str[len-1] = '\0';
    return str;
}

void *push_data(void *rbuf)
{
    printf("enter push thread\n");
    unsigned int total, single, len;

    for (total = 0; total < TEST_STR_LEN;) {
        single = get_random_size(SINGLE_MAX);
        single = min(single, TEST_STR_LEN - total);
        // 阻塞的写入所有in_buf中数据
        len = rbuf_put((ring_buf_t *)rbuf
                , input_str + total
                , single);
        total += len;
        //printf("write <size:%d> data\n", n);
    }
    return NULL;
}

void *pop_data(void *rbuf)
{
    printf("enter pop thread\n");
    unsigned int total, len;
    // test ring buffer auto resize
    sleep(1);
    for (total = 0; total < TEST_STR_LEN; ) {
        len = rbuf_get((ring_buf_t *)rbuf
                , output_str + total
                , RECV_BUF_LEN - total);
        total += len;
        //printf("read <size:%d> data\n", len);        
    }
    return NULL;
}

int main()
{
    /*uchar_8 *buf = (uchar_8 *)malloc(R_BUF_SIZE);*/
    /*ring_buf_t *rbuf = rbuf_init(R_BUF_SIZE, NULL, buf);*/
    mempool_t *mempool = create_mempool(MEMPOOL_SIZE);
    ring_buf_t *rbuf = rbuf_init(RINGBUFFER_SIZE, mempool, NULL);
    rand_str(input_str, TEST_STR_LEN);

    if (!rbuf) return -1;

    pthread_t thread_1;
    pthread_t thread_2; 
    int ret = pthread_create(&thread_1, NULL, push_data, rbuf);
    if (ret != 0) {
        return -1;
    }

    ret = pthread_create(&thread_2, NULL, pop_data, rbuf);
    if (ret != 0) {
        return -1;
    }
    
    pthread_join(thread_1, NULL);
    pthread_join(thread_2, NULL);

    assert(strcmp(input_str, output_str) == 0);
    printf("test OK!\n");

    /*buf = (uchar_8 *)rbuf_fini(rbuf);*/
    destroy_mempool(mempool);
}
