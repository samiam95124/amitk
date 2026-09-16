/*******************************************************************************
*                                                                              *
*                      Standard definitions for Petit_ami                      *
*                                                                              *
* In most, but not all cases, C will simply treat duplicate definitions as a   *
* no-op if they are identical (the present exception being boolean). However,  *
* for the cases that is not so, we present standard used definitions here.     *
*                                                                              *
*******************************************************************************/

#ifndef __LOCALDEFS_H__
#define __LOCALDEFS_H__

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FALSE 0
#define TRUE  1

#define BIT(b) (1<<b) /* set bit from bit number */
#define BITMSK(b) (~BIT(b)) /* mask out bit number */

typedef char* string;  /* general string type */

/* Ami's long is the machine word: 32 bits on a 32 bit machine, 64 on a 64 bit
   one. 64 bit Windows keeps long at 32 (LLP64), so the API and everything
   built on it spell the type ami_long, which is long long there and long
   everywhere else. It prints under %lld through AMI_LONG_CAST, below. long long
   stays long long: it is 64 bits on every host. */
#ifdef _WIN64
typedef long long          ami_long;
typedef unsigned long long ami_ulong;
#else
typedef long               ami_long;
typedef unsigned long      ami_ulong;
#endif

/* Printing an ami_long: the value goes under %lld (%llu, %llx) through this
   cast, which is the 64 bit type on every host, so the format checker is
   satisfied everywhere and the width is the same everywhere. */
#define AMI_LONG_CAST  (long long)
#define AMI_ULONG_CAST (unsigned long long)

/* The largest ami_long, which is the full scale of the values the API ratios to
   the machine word, such as a joystick axis: the largest of the type ami_long
   is on the host. */
#ifdef _WIN64
#define AMI_LONG_MAX LLONG_MAX
#else
#define AMI_LONG_MAX LONG_MAX
#endif
typedef unsigned char byte; /* byte */

#ifdef __cplusplus
}
#endif

#endif /* __LOCALDEFS_H__ */
