#include <string.h>
#include <stdlib.h> 
#include <time.h>  
#include <assert.h>

#include "mempool.h"

#define MEMPOOL_SIZE    1024
#define MAX_BLOCK_SIZE  1500
#define TEST_TIMES      50

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

typedef struct memory_pair
{
    uchar_8*    from_pool;
    uchar_8*    from_malloc;
} memory_pair;

memory_pair test_data[TEST_TIMES];
char random_str[MAX_BLOCK_SIZE];

int main()
{
    srand((unsigned)time(NULL));
    mempool_t *pool = create_mempool(MEMPOOL_SIZE);

    int i,size;
    for (i = 0; i < TEST_TIMES; i++)
    {
        size = get_random_size(MAX_BLOCK_SIZE);
        test_data[i].from_pool = (uchar_8 *)pool_alloc(pool, size);
        test_data[i].from_malloc = (uchar_8 *)naive_alloc(size);
        strncpy(test_data[i].from_malloc, rand_str(random_str, size), size);
        strncpy(test_data[i].from_pool, test_data[i].from_malloc, size);
    }

    for (i = 0; i < TEST_TIMES; i++)
    {
        assert(strcmp(test_data[i].from_pool, test_data[i].from_malloc) == 0);
    }

    destroy_mempool(pool);
    return 0;
}
