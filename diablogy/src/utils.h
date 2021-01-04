#ifndef __vpath_utils_h__
#define __vpath_utils_h__

#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include "libc.h"

#ifdef __cplusplus
extern "C"
{
#endif

    extern volatile int __debug_log_fd;
    void reinit_debug_log(void);
    void vpath_get_log_file_name (char *buf, int buf_len, const char *log_file_prefix);
    void __debug_print (const char *buf);
    void __debug_panic (const char *buf);
    void console_print (const char * s);
    int printLocalAddr (char *buf, int printLimit, int fd);
    int printAddr (char *buf, int printLimit, const struct sockaddr * ap, socklen_t addrlen);
    int printAddrWithType (char *buf, int printLimit, const struct sockaddr * ap, socklen_t addrlen);
    int printRemoteAddr (char *buf, int printLimit, int fd);
    const char *afString (int addrFamily);
    // Program name, from crt0.
    extern char *__progname;
    void pause_for_debug (void);

#ifdef __cplusplus
}
#endif

#define DEFAULT_LOG_FD 2     // STDERR
#define uint8     unsigned char
#define uint16    unsigned short int
#define int16     short int
#define uint32    unsigned int
#define int32     int
#define uint64    unsigned long long int
#define int64     long long int

#ifndef __IN_VPATH__
#  define __IN_VPATH__ 0
#endif

#ifndef ABS
#  define ABS(x)    ((x)>=0 ? (x) : -(x))
#endif

#ifndef MIN
#  define MIN(x,y)    ((x)>(y)? (y) : (x))
#endif

#ifndef MAX
#  define MAX(x,y)    ((x)>(y)? (x) : (y))
#endif

#ifndef ROUND_UP
#  define ROUND_UP(x, base)     (((x) + (base) - 1) & ~((base) - 1))
// ROUND_UP
#endif
#define ROUND_DN(x, base)       ((x) & ~((base) - 1))

#define UNUSED_LABEL(x) {if(0) goto x;}
#ifndef UNUSED
# define UNUSED(x) (void)(x)
#endif

#ifndef TRUE
#  define TRUE    1
#endif

#ifndef FALSE
#  define FALSE   0
#endif

#ifndef DEBUG
#  define DEBUG 0
#endif

#define PER_PROCESS_LOG         0
#define MERGED_PROCESS_LOG      1
#define STDERRLOG               2
#define SYSLOG                  3
extern int DEBUG_LOG_MEDIA;

#define PRINT_BUF_SIZE  2048

#define logerr(format,...) \
    ( \
    { \
        char ____buf[PRINT_BUF_SIZE]; \
        snprintf (____buf, PRINT_BUF_SIZE-1, __FILE__ ":%d " format, __LINE__, ##__VA_ARGS__); \
        ____buf[PRINT_BUF_SIZE-1] = 0; \
        syslog (LOG_INFO, "%s", ____buf); \
        libc_write (2, ____buf, strlen(____buf)); \
    })

#define console_print(format,...) \
    ( \
    { \
        char ____buf[PRINT_BUF_SIZE]; \
        snprintf (____buf, PRINT_BUF_SIZE-1, format, ##__VA_ARGS__); \
        ____buf[PRINT_BUF_SIZE-1] = 0; \
        libc_write (2, ____buf, strlen(____buf)); \
    })

#define panic(format,...) \
    ( \
    { \
        char ____buf[PRINT_BUF_SIZE]; \
        snprintf (____buf, PRINT_BUF_SIZE-1, __FILE__ ":%d " format, __LINE__, ##__VA_ARGS__); \
        ____buf[PRINT_BUF_SIZE-1] = 0; \
        __debug_panic (____buf); \
    })

#define debug_print(format,...) \
    ( \
    { \
        char ____buf[PRINT_BUF_SIZE]; \
        snprintf (____buf, PRINT_BUF_SIZE-1, format, ##__VA_ARGS__); \
        ____buf[PRINT_BUF_SIZE-1] = 0; \
        __debug_print (____buf); \
    })

#if DEBUG
#  define trace(format,...)   debug_print(__FILE__ ":%d " format, __LINE__, ##__VA_ARGS__)
#  define ASSERT    assert
#else
#  define trace(format, ...)
#  define ASSERT(cond)
#endif

#define TALLOC(type) ((type*)malloc(sizeof(type)))
#define NALLOC(type,n) ((type*)malloc(sizeof(type)*n))

#define isBigEndianCPU() \
    ( \
    { \
        const int endian = 0x00010203; \
        const char *ep = (char *) &endian; \
        ep == 0; \
    })

#define SNPRINTF snprintf

#endif
