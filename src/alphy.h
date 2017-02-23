#ifndef ALPHY_H
#define ALPHY_H

#include "stddef.h"
#include "stdbool.h"
#include "errorcode.h"

// Check GCC
#if __GNUC__
#if __x86_64__ || __ppc64__
#define ENV64BIT
#else
#define ENV32BIT
#endif
#endif

#define uchar_8 unsigned char

#ifdef ENV64BIT
#define int_32  int
#define int_64  long
#define uint_32 unsigned int
#define uint_64 unsigned long
#endif

#ifdef ENV32BIT
#define int_32  int
#define int_64  long long
#define uint_32 unsigned int
#define uint_64 unsigned long long
#endif

#define min(a,b) a<b?a:b
#define max(a,b) a>b?a:b

typedef struct mempool_data_t mempool_data_t;
typedef struct mempool_t mempool_t;
typedef struct mempool_large_t mempool_large_t; 
typedef struct ring_buf_t ring_buf_t;


#define DEBUG_MEMPOOL
#define DEBUG_RINGBUFFER

#endif 
