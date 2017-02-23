#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "mempool.h"
#include "errorcode.h"


static size_t _allign_32(size_t size);
static void *_pool_alloc_small(mempool_t *pool, size_t size);
static void *_pool_alloc_large(mempool_t *pool, size_t size);
static void *_pool_alloc_block(mempool_t *pool, size_t size);

static size_t _allign_32(size_t size)
{
    if (size % 64 == 0) return size;
    return ((size >> 5) + 1) << 5;
}

void *naive_alloc(size_t size)
{
    return malloc(size);
}

mempool_t *create_mempool(size_t size)
{
    size = _allign_32(size);
    mempool_t *p = (mempool_t *)naive_alloc(size);
    if (p == NULL) return p;

    p->data.last = (uchar_8 *)p + sizeof(mempool_t);
    p->data.end = (uchar_8 *)p + size;
    p->data.next = NULL;
    p->data.failed = 0;

    size = size - sizeof(mempool_t);
    p->max = min(size, MAX_ALLOC_FROM_POOL);

    p->current = p;
    p->large = NULL;

    return p;
}

void destroy_mempool(mempool_t *pool)
{
#ifdef DEBUG_MEMPOOL
    static int large_cnt = 0;
    static int block_cnt = 0;
#endif

    mempool_t          *p, *n;
    mempool_large_t    *l;

    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
#ifdef DEBUG_MEMPOOL
            printf("a new large data has been freed <%d>\n", ++large_cnt);
#endif            
            naive_free(l->alloc);
        }
    }

    for (p = pool, n = pool->data.next; ; p = n, n = n->data.next) {
#ifdef DEBUG_MEMPOOL
        printf("a new block data has been freed <%d>\n", ++block_cnt);
#endif
        naive_free(p);

        if (n == NULL) {
            break;
        }
    }
}

void reset_mempool(mempool_t *pool)
{
    mempool_t       *p;
    mempool_large_t *l;

    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            naive_free(l->alloc);
        }
    }

    for (p = pool; p; p = p->data.next) {
        p->data.last = (u_char *) p + sizeof(mempool_t);
        p->data.failed = 0;
    }

    pool->current = pool;
    pool->large = NULL;
}

void *pool_alloc(mempool_t *pool, size_t size)
{
    if (size <= pool->max) {
        return _pool_alloc_small(pool, size);
    }

    return _pool_alloc_large(pool, size);
}

void *pool_calloc(mempool_t *pool, size_t size)
{
    void *p;

    p = pool_alloc(pool, size);
    if (p) {
        memset(p, 0, size);
    }
    return p;
}

void *_pool_alloc_small(mempool_t *pool, size_t size)
{
    uchar_8     *m;
    mempool_t   *p;

    p = pool->current;

    // 1. current之后可能有新的block,但是当前block未满之前(4次分配失败),不切换current指针
    // 2. reset后的mempool可能有多个空block
    do {
        m = p->data.last;

        if ((size_t) (p->data.end - m) >= size) {
            p->data.last = m + size;

            return m;
        }

        p = p->data.next;
    } while (p);

    return _pool_alloc_block(pool, size);
}

void *_pool_alloc_block(mempool_t *pool, size_t size)
{
#ifdef DEBUG_MEMPOOL    
    static uint_32 cnt = 0;
#endif

    uchar_8     *m;
    size_t      psize;
    mempool_t   *p;
    mempool_t   *n;     //new

    psize = (size_t) (pool->data.end - (uchar_8 *) pool);

    m = (uchar_8 *)naive_alloc(psize);
    if (m == NULL) {
        return NULL;
    }

    n = (mempool_t *) m;

    n->data.end = m + psize;
    n->data.next = NULL;
    n->data.failed = 0;

    m += sizeof(mempool_data_t);
    n->data.last = m + size;

    for (p = pool->current; p->data.next; p = p->data.next) {
        if (p->data.failed++ > 4) {
#ifdef DEBUG_MEMPOOL    
            int size = pool->data.end - (uchar_8 *)pool - sizeof(mempool_t);
            int used = pool->current->data.last - (uchar_8 *)pool->current - sizeof(mempool_t);
            printf("current pool invalid, <%d used Byte/ %d total Byte>\n", used, size);
#endif
            pool->current = p->data.next;
        }
    }

    p->data.next = n;
#ifdef DEBUG_MEMPOOL    
    printf("a new block has been allocted <%d>\n", ++cnt);
#endif

    return m;
}

// 使用LIFO管理大内存块
void *_pool_alloc_large(mempool_t *pool, size_t size)
{
#ifdef DEBUG_MEMPOOL    
    static int          cnt = 0;
#endif

    void                *p;
    mempool_large_t     *large;
    int     n;

    p = naive_alloc(size);
    if (p == NULL) {
        return NULL;
    }

#ifdef DEBUG_MEMPOOL    
    printf("a new large data has been allocted <%d>\n", ++cnt);
#endif
    
    n = 0;

    for (large = pool->large; large; large = large->next) {
        if (large->alloc == NULL) {
            large->alloc = p;
            return p;
        }

        if (n++ > 5)
            break;
    }

    large = (mempool_large_t *)_pool_alloc_small(pool, sizeof(mempool_large_t));
    if (large == NULL) {
        naive_free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}

// only for large block
int_32 pool_free(mempool_t *pool, void *p)
{
    mempool_large_t  *l;

    for (l = pool->large; l; l = l->next) {
        if (p == l->alloc) {
            naive_free(l->alloc);
            l->alloc = NULL;

            return ALPHY_OK;
        }
    }

    return ALPHY_DECLINED;
}
