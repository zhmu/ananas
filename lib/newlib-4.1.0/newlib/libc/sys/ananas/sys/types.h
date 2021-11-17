#ifndef _SYS_TYPES_H_
#define _SYS_TYPES_H_

#include <ananas/types.h>

// Standard types
typedef __uint8_t        uint8_t;
typedef __uint16_t       uint16_t;
typedef __uint32_t       uint32_t;
typedef __uint64_t       uint64_t;
typedef __int8_t         int8_t;
typedef __int16_t        int16_t;
typedef __int32_t        int32_t;
typedef __int64_t        int64_t;

// Kernel types
typedef __blkcnt_t       blkcnt_t;
typedef __blksize_t      blksize_t;
typedef __blocknr_t      blocknr_t;
typedef __clock_t        clock_t;
typedef __clockid_t      clockid_t;
typedef __dev_t          dev_t;
typedef __gid_t          gid_t;
typedef __id_t           id_t;
typedef __ino_t          ino_t;
typedef __key_t          key_t;
typedef __mode_t         mode_t;
typedef __nlink_t        nlink_t;
typedef __off_t          off_t;
typedef __pid_t          pid_t;
typedef __socklen_t      socklen_t;
typedef __suseconds_t    suseconds_t;
typedef __useconds_t     useconds_t;
typedef __tick_t         tick_t;
typedef __time_t         time_t;
typedef __uid_t          uid_t;

typedef __SIZE_TYPE__    size_t;
#define unsigned signed
typedef __SIZE_TYPE__    ssize_t;
#undef unsigned

typedef __uint8_t        u_int8_t;
typedef __uint16_t       u_int16_t;
typedef __uint32_t       u_int32_t;
typedef __uint64_t       u_int64_t;

typedef __intptr_t       intptr_t;
typedef __uintptr_t      uintptr_t;

typedef unsigned char    u_char;
typedef unsigned short   u_short;
typedef unsigned int     u_int;
typedef unsigned long    u_long;

typedef __int64_t        sbintime_t;
typedef char*            caddr_t;

typedef unsigned long    timer_t;

#endif
