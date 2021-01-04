#ifndef __libc_wrapper_h__
#define __libc_wrapper_h__

#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <sys/resource.h>
//#include <sys/wait.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <poll.h>
#include <sys/select.h>
#include <sys/epoll.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __IN_VPATH__
#  define libc_write  write
#  define libc_read   read
#  define libc_close  close
#  define libc_socket   socket
#  define libc_connect  connect
#  define libc_dup  dup
#  define libc_open  open
#  define libc_dup2  dup2
#  define libc_syslog  syslog
#else
    extern void (*libc_res_nclose) (void *p);
    extern void (*libc_res_iclose) (void *statp, bool free_addr);
    extern void (*libc_res_close) ();
    extern int (*libc_dup) (int oldfd);
    extern int (*libc_dup2) (int oldfd, int newfd);
    extern pid_t (*libc_fork) (void);
    extern pid_t (*libc_vfork) (void);
    extern int (*libc_execve) (const char *filename, char *const argv[], char *const envp[]);
    extern int (*libc_execvp)(const char *file, char *const argv[]);
    extern int (*libc_execv)(const char *path, char *const argv[]);
    extern void (*libc_exit) (int status);
    extern void (*libc__exit) (int status);
    extern void (*libc__Exit) (int status);
    extern int (*libc_accept) (int sockfd, struct sockaddr * remote, socklen_t * addrlen);
    extern int (*libc_socket) (int domain, int type, int protocol);
    extern int (*libc_connect) (int sockfd, const struct sockaddr * serv_addr, socklen_t addrlen);
    extern int (*libc_close) (int fd);
    extern int (*libc_bind) (int sockfd, const struct sockaddr * my_addr, socklen_t addrlen);
    extern ssize_t (*libc_recv) (int s, void *buf, size_t len, int flags);
    extern ssize_t (*libc_recvfrom) (int s, void *buf, size_t len, int flags, struct sockaddr * from, socklen_t * fromlen);
    extern ssize_t (*libc_recvmsg) (int s, struct msghdr * msg, int flags);
    extern ssize_t (*libc_read) (int fd, void *buf, size_t count);
    extern ssize_t (*libc_readv) (int fd, const struct iovec * iov, int iovcnt);
    extern ssize_t (*libc_send) (int s, const void *buf, size_t len, int flags);
    extern ssize_t (*libc_sendto) (int s, const void *buf, size_t len, int flags, const struct sockaddr * to, socklen_t tolen);
    extern ssize_t (*libc_sendmsg) (int s, const struct msghdr * msg, int flags);
    extern ssize_t (*libc_writev) (int fd, const struct iovec * iov, int iovcnt);
    extern ssize_t (*libc_write) (int fd, const void *buf, size_t count);
    extern ssize_t (*libc_sendfile) (int out_fd, int in_fd, off_t * offset, size_t count);
    extern pid_t (*libc_wait) (int *status);
    extern pid_t (*libc_waitpid) (pid_t pid, int *status, int options);
    extern pid_t (*libc_wait3) (int *status, int options, struct rusage * rusage);
    extern pid_t (*libc_wait4) (pid_t pid, int *status, int options, struct rusage * rusage);
    extern int (*libc_shutdown) (int fd, int how);
    extern int (*libc_pthread_create) (pthread_t * thread, const pthread_attr_t * attr, void *(*start_routine) (void *), void *arg);
    extern void (*libc_pthread_exit) (void *value_ptr);
    extern int (*libc_ppoll) (struct pollfd * fds, nfds_t nfds, const struct timespec * timeout, const sigset_t * sigmask);
    extern int (*libc_poll) (struct pollfd * fds, nfds_t nfds, int timeout);
    extern int (*libc_epoll_wait)(int epfd, struct epoll_event *events, int maxevents, int timeout);
    extern int (*libc_epoll_pwait)(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask);
    extern int (*libc_epoll_create)(int size);
    extern int (*libc_epoll_create1)(int flag);
    extern int (*libc_epoll_ctl)(int epfd, int op, int fd, struct epoll_event *event);
    extern int (*libc_select)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
    extern int (*libc_pselect)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask);
    extern int (*libc_fcntl)(int fd, int cmd, ...);
    extern int (*libc_socketpair)(int domain, int type, int protocol, int sv[2]);
    extern int (*libc_open)(const char *filename, int mode, ...);
    extern int (*libc_open64)(const char *filename, int mode, ...);
    extern FILE* (*libc_fopen) (const char *filename, const char *opentype);
    extern FILE* (*libc_fopen64) (const char *filename, const char *opentype);
    extern int (*libc_fclose) (FILE *fp);
    extern FILE* (*libc_freopen) (const char *filename, const char *opentype, FILE* stream);
    extern FILE* (*libc_freopen64) (const char *filename, const char *opentype, FILE* stream);
    extern int (*libc_creat)(const char *pathname, mode_t mode);
    extern int (*libc_creat64)(const char *pathname, mode_t mode);
    extern FILE* (*libc_fdopen) (int fildes, const char *opentype);
    extern void (*libc_syslog) (int priority, const char *format, ...);
    extern void (*libc_vsyslog) (int priority, const char *format, va_list ap);
    extern size_t (*libc_fwrite) (const void *ptr, size_t size, size_t nmemb, FILE *stream);
    extern size_t (*libc_fwrite_unlocked) (const void *ptr, size_t size, size_t nmemb, FILE *stream);
    extern ssize_t (*libc_pwrite64) (int fd, const void *buf, size_t count, off64_t offset);
    extern ssize_t (*libc_pwrite) (int fd, const void *buf, size_t count, off_t offset);
    extern ssize_t (*libc_pwritev) (int fd, const struct iovec *iov, int iovcnt, off_t offset);
    extern ssize_t (*libc_pwritev64) (int fd, const struct iovec *iov, int iovcnt, off_t offset);
//    extern void *(*libc_dlopen)(const char *filename, int flag);

#endif

#ifdef __cplusplus
}
#endif
#endif
