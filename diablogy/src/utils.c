//#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include "utils.h"
#include "libc.h"

volatile int __debug_log_fd = DEFAULT_LOG_FD;
volatile int __debug_log_fd_to_use = -1;
int DEBUG_LOG_MEDIA = SYSLOG;

void vpath_get_log_file_name (char *buf, int buf_len, const char *log_file_prefix)
{
    char *log_path = getenv ("VPATH_LOG_PATH");
    if (log_path == NULL) log_path = (char *) "/tmp/vpath";

    if (mkdir (log_path, S_IRWXU | S_IRWXG | S_IRWXO) == 0)
        chmod (log_path, S_IRWXU | S_IRWXG | S_IRWXO);

    struct timespec ts;
    clock_gettime (CLOCK_REALTIME, &ts);

    char name[1024];
    char *p = __progname;
    char *q = name;
    while (*p != 0) {
        if (*p == ' ') *q = '-';
        else if (*p == '/') *q = '-';
        else if (*p == ';') *q = '-';
        else *q = *p;
        p++;
        q++;
    }
    *q = 0;

    snprintf (buf, buf_len, "%s/%s-%s-pid%d-s%lu-%lu", log_path, log_file_prefix, name, getpid (), ts.tv_sec, ts.tv_nsec);
}

static pthread_mutex_t init_log_lock = PTHREAD_MUTEX_INITIALIZER;
static void init_per_process_log (void)
{
    char buf[1024];

    pthread_mutex_lock (&init_log_lock);

    if (__debug_log_fd != DEFAULT_LOG_FD) goto EXIT;

    vpath_get_log_file_name (buf, 1024, "debug.log");

    __debug_log_fd = libc_open (buf, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (__debug_log_fd < 0) {
        logerr ("Cannot create log file %s\n", buf);
        goto EXIT;
    }

    if (__debug_log_fd_to_use >= 0) {
        if (dup2(__debug_log_fd, __debug_log_fd_to_use) != __debug_log_fd_to_use) {
            libc_close (__debug_log_fd);
            logerr ("Cannot set debug log fd to %d\n", __debug_log_fd_to_use);
            goto EXIT;
        }
        __debug_log_fd = __debug_log_fd_to_use;
    }
    else if (__debug_log_fd == DEFAULT_LOG_FD) {
        int fd = libc_dup (__debug_log_fd);
        libc_close (__debug_log_fd);
        if (fd < 0) {
            logerr ("dup() error\n");
            __debug_log_fd = -1;
            goto EXIT;
        }
        __debug_log_fd = fd;
    }

    if (fcntl (__debug_log_fd, F_SETFD, FD_CLOEXEC) != 0) {
        logerr ("ERROR: cannot set FD_CLOEXEC on log file `%s'\n", buf);
        libc_close (__debug_log_fd);
        __debug_log_fd = -1;
        goto EXIT;
    }

    EXIT:
    pthread_mutex_unlock (&init_log_lock);
}

static void init_file_sock_log (const char *file_sock_path)
{
    pthread_mutex_lock (&init_log_lock);
    if (__debug_log_fd != DEFAULT_LOG_FD)
        goto EXIT;

    __debug_log_fd = libc_socket (AF_FILE, SOCK_DGRAM, 0);
    if (__debug_log_fd < 0) {
        logerr ("Cannot create socket.\n");
        goto EXIT;
    }

    if (__debug_log_fd_to_use >= 0) {
        if (libc_dup2(__debug_log_fd, __debug_log_fd_to_use) != __debug_log_fd_to_use) {
            libc_close (__debug_log_fd);
            logerr ("Cannot set debug log fd to %d\n", __debug_log_fd_to_use);
            goto EXIT;
        }
        __debug_log_fd = __debug_log_fd_to_use;
    }
    else if (__debug_log_fd == DEFAULT_LOG_FD) {
        int fd = libc_dup (__debug_log_fd);
        libc_close (__debug_log_fd);
        if (fd < 0) {
            logerr ("dup() error\n");
            __debug_log_fd = -1;
            goto EXIT;
        }
        __debug_log_fd = fd;
    }

 
    if (fcntl (__debug_log_fd, F_SETFD, FD_CLOEXEC) != 0) {
        logerr ("ERROR: cannot set FD_CLOEXEC on socket");
        libc_close (__debug_log_fd);
        __debug_log_fd = -1;
        goto EXIT;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_FILE;
    strncpy (addr.sun_path, file_sock_path, sizeof (addr.sun_path));
    if (libc_connect (__debug_log_fd, (struct sockaddr *) &addr, sizeof (addr)) != 0) {
        logerr ("ERROR: cannot connect to %s\n", file_sock_path);
        libc_close (__debug_log_fd);
        __debug_log_fd = -1;
    }

    EXIT:
    pthread_mutex_unlock (&init_log_lock);
}

void reinit_debug_log (void)
{
    if (DEBUG_LOG_MEDIA == PER_PROCESS_LOG && __debug_log_fd != DEFAULT_LOG_FD && __debug_log_fd >= 0) {
        libc_close (__debug_log_fd);
        __debug_log_fd = DEFAULT_LOG_FD;
    }
}

void __debug_print (const char *buf)
{
    if (DEBUG_LOG_MEDIA == SYSLOG) {
        libc_syslog (LOG_INFO, "%s", buf);
        return;
    }

    int err = 0;
RETRY:
    if (DEBUG_LOG_MEDIA == PER_PROCESS_LOG) {
        if (__debug_log_fd == DEFAULT_LOG_FD) init_per_process_log ();
        if (__debug_log_fd < 0) return;
    }
    else if (DEBUG_LOG_MEDIA == MERGED_PROCESS_LOG) {
        if (__debug_log_fd == DEFAULT_LOG_FD) init_file_sock_log ("/tmp/vpath.log.sock");
        if (__debug_log_fd < 0) return;
    }

    int len = strlen (buf);
    const char *p = buf;
    while (len > 0) {
        int r = libc_write (__debug_log_fd, p, len);
        if (r < 0) {
            if (err > 0) return;

            // Retry on first error, because the application (e.g., ssh) may close all 
            // files, including AppCloak debugging log. Retry will open it again.
            __debug_log_fd = DEFAULT_LOG_FD;
            err++;
            goto RETRY;
        }
        p += r;
        len -= r;
    }
}

void __debug_panic (const char *buf)
{
    __debug_print (buf);
    char dir[256];

    if (errno != 0) __debug_print (strerror (errno));

    logerr ("Exit on error. See info in the log file.\nDump a core in directory %s.\n", getcwd (dir, 256));
    abort ();
    exit (1);
}

const char *afString (int addrFamily)
{
    switch (addrFamily) {
        case AF_UNSPEC: return "AF_UNSPEC";
        case AF_FILE: return "AF_FILE";
        case AF_INET: return "AF_INET";
        case AF_AX25: return "AF_AX25";
        case AF_IPX: return "AF_IPX";
        case AF_APPLETALK: return "AF_APPLETALK";
        case AF_NETROM: return "AF_NETROM";
        case AF_BRIDGE: return "AF_BRIDGE";
        case AF_ATMPVC: return "AF_ATMPVC";
        case AF_X25: return "AF_X25";
        case AF_INET6: return "AF_INET6";
        case AF_ROSE: return "AF_ROSE";
        case AF_DECnet: return "AF_DECnet";
        case AF_NETBEUI: return "AF_NETBEUI";
        case AF_SECURITY: return "AF_SECURITY";
        case AF_KEY: return "AF_KEY";
        case AF_NETLINK: return "AF_NETLINK";
        case AF_PACKET: return "AF_PACKET";
        case AF_ASH: return "AF_ASH";
        case AF_ECONET: return "AF_ECONET";
        case AF_ATMSVC: return "AF_ATMSVC";
        case AF_SNA: return "AF_SNA";
        case AF_IRDA: return "AF_IRDA";
        case AF_PPPOX: return "AF_PPPOX";
        case AF_WANPIPE: return "AF_WANPIPE";
        case AF_BLUETOOTH: return "AF_BLUETOOTH";
        case AF_MAX: return "AF_MAX";
        default: return "AF_unknown";
    }
}

int printAddr (char *buf, int printLimit, const struct sockaddr * ap, socklen_t addrlen)
{
    if (ap == NULL) {
        buf[0] = '.';
        buf[1] = 0;
        return 1;
    }

    if (ap->sa_family == AF_INET) {
        if (addrlen < sizeof (struct sockaddr_in)) return snprintf (buf, printLimit, "%s", afString (ap->sa_family));

        char addr[256];
        addr[0] = 0;                              // Set string to null in case that inet_ntop fails.
        struct sockaddr_in *r = (struct sockaddr_in *) ap;
        inet_ntop (ap->sa_family, &r->sin_addr, addr, 256);
        return snprintf (buf, printLimit, "%s %d", addr, ntohs (r->sin_port));
    }

    if (ap->sa_family == AF_INET6) {
        if (addrlen < sizeof (struct sockaddr_in6)) return snprintf (buf, printLimit, "%s", afString (ap->sa_family));

        char addr[256];
        addr[0] = 0; // Set string to null in case that inet_ntop fails.
        struct sockaddr_in6 *r = (struct sockaddr_in6 *) ap;
        inet_ntop (ap->sa_family, &r->sin6_addr, addr, 256);
        return snprintf (buf, printLimit, "%s %d", addr, ntohs (r->sin6_port));
    }

    if (ap->sa_family == AF_FILE) {
        if (addrlen <= sizeof (ap->sa_family)) return snprintf (buf, printLimit, "%s", afString (ap->sa_family));

        strncpy (buf, ((struct sockaddr_un *) ap)->sun_path, printLimit-1);
        return strlen (buf);
    }

    return snprintf (buf, printLimit, "%s", afString (ap->sa_family));
}

int printAddrWithType (char *buf, int printLimit, const struct sockaddr * ap, socklen_t addrlen)
{
    if (ap == NULL) {
        buf[0] = '.';
        buf[1] = 0;
        return 1;
    }

    if (ap->sa_family == AF_INET) {
        if (addrlen < sizeof (struct sockaddr_in)) return snprintf (buf, printLimit, "%s", afString (ap->sa_family));

        char addr[256];
        addr[0] = 0;                              // Set string to null in case that inet_ntop fails.
        struct sockaddr_in *r = (struct sockaddr_in *) ap;
        inet_ntop (ap->sa_family, &r->sin_addr, addr, 256);
        return snprintf (buf, printLimit, "%s %s %d", afString(ap->sa_family), addr, ntohs (r->sin_port));
    }

    if (ap->sa_family == AF_INET6) {
        if (addrlen < sizeof (struct sockaddr_in6)) return snprintf (buf, printLimit, "%s", afString (ap->sa_family));

        char addr[256];
        addr[0] = 0; // Set string to null in case that inet_ntop fails.
        struct sockaddr_in6 *r = (struct sockaddr_in6 *) ap;
        inet_ntop (ap->sa_family, &r->sin6_addr, addr, 256);
        return snprintf (buf, printLimit, "%s %s %d", afString(ap->sa_family), addr, ntohs (r->sin6_port));
    }

    if (ap->sa_family == AF_FILE) {
        if (addrlen <= sizeof (ap->sa_family)) return snprintf (buf, printLimit, "%s", afString (ap->sa_family));
        strncpy (buf, ((struct sockaddr_un *) ap)->sun_path, printLimit-1);
        return strlen (buf);
    }

    return snprintf (buf, printLimit, "%s", afString (ap->sa_family));
}

int printLocalAddr (char *buf, int printLimit, int fd)
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof (addr);

    if (getsockname (fd, (struct sockaddr *) &addr, &len) != 0) {
		sprintf(buf,"0.0.0.0 0");
        buf[9] = 0;
        return 9;
    }

    return printAddrWithType (buf, printLimit, (struct sockaddr *) & addr, len);
}

int printRemoteAddr (char *buf, int printLimit, int fd)
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof (addr);

    if (getpeername (fd, (struct sockaddr *) &addr, &len) != 0) {
		sprintf(buf,"0.0.0.0 0");
        buf[9] = 0;
        return 9;
    }

    return printAddr (buf, printLimit, (struct sockaddr *) & addr, len);
}

void pause_for_debug (void)
{
    char buf[256];
    printf ("Press any key to continue process %d\n", getpid());
    char *p = fgets(buf, 256, stdin);
    UNUSED (p);
}
