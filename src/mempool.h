#ifndef MEMPOOL_H
#define MEMPOOL_H

// size_t
#include "alphy.h"

typedef struct mempool_data_t mempool_data_t;
typedef struct mempool_t mempool_t;
typedef struct mempool_large_t mempool_large_t; 

#define MAX_ALLOC_FROM_POOL 1024

struct mempool_data_t
{
    uchar_8     *last; 
    uchar_8     *end; 
    mempool_t   *next;
    uint_32     failed;
};

struct mempool_large_t
{
    mempool_large_t *next;
    void *alloc;
};

struct mempool_t
{
    mempool_data_t  data;
    mempool_t       *current;
    mempool_large_t *large;

    size_t          max;
};


void *naive_alloc(size_t size);
#define naive_free free

mempool_t *create_mempool(size_t size);
void destroy_mempool(mempool_t *pool);
void reset_mempool(mempool_t *pool);

void *pool_alloc(mempool_t *pool, size_t size);
void *pool_calloc(mempool_t *pool, size_t size);
int_32 pool_free(mempool_t *pool, void *p);

#endif 
