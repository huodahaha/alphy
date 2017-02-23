#ifndef ALPHY_H
#define ALPHY_H

#include "stddef.h"

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

#endif 
