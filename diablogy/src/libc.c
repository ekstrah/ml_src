#ifdef __IN_VPATH__

#define __USE_GNU
#include <dlfcn.h>
#include "libc.h"

//#define RTLD_NEXT      ((void *) -1l)

typedef void (*func) (void);

void (*libc_res_nclose) (void *p);
void (*libc_res_close) ();
int (*libc_dup) (int oldfd);
int (*libc_dup2) (int oldfd, int newfd);
pid_t (*libc_fork) (void);
pid_t (*libc_vfork) (void);
int (*libc_execve) (const char *filename, char *const argv[], char *const envp[]);
int (*libc_execvp) (const char *file, char *const argv[]);
int (*libc_execv) (const char *path, char *const argv[]);
void (*libc_exit) (int status);
void (*libc__exit) (int status);
void (*libc__Exit) (int status);
int (*libc_accept) (int sockfd, struct sockaddr * remote, socklen_t * addrlen);
int (*libc_socket) (int domain, int type, int protocol);
int (*libc_connect) (int sockfd, const struct sockaddr * serv_addr, socklen_t addrlen);
int (*libc_close) (int fd);
int (*libc_bind) (int sockfd, const struct sockaddr * my_addr, socklen_t addrlen);
ssize_t (*libc_recv) (int s, void *buf, size_t len, int flags);
ssize_t (*libc_recvfrom) (int s, void *buf, size_t len, int flags, struct sockaddr * from, socklen_t * fromlen);
ssize_t (*libc_recvmsg) (int s, struct msghdr * msg, int flags);
ssize_t (*libc_read) (int fd, void *buf, size_t count);
int (*libc_open) (const char *filename, int mode, ...);
int (*libc_open64) (const char *filename, int mode, ...);
FILE* (*libc_fopen) (const char *filename, const char *opentype);
FILE* (*libc_fopen64) (const char *filename, const char *opentype);
int (*libc_fclose) (FILE * fp);
FILE* (*libc_freopen) (const char *filename, const char *opentype, FILE* stream);
FILE* (*libc_freopen64) (const char *filename, const char *opentype, FILE* stream);
int (*libc_creat) (const char *pathname, mode_t mode);
int (*libc_creat64) (const char *pathname, mode_t mode);
FILE* (*libc_fdopen) (int fildes, const char *opentype);
void (*libc_syslog) (int priority, const char *format, ...);
void (*libc_vsyslog) (int priority, const char *format, va_list ap);
size_t (*libc_fwrite) (const void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t (*libc_fwrite_unlocked) (const void *ptr, size_t size, size_t nmemb, FILE *stream);
ssize_t (*libc_pwrite64) (int fd, const void *buf, size_t count, off64_t offset);
ssize_t (*libc_pwrite) (int fd, const void *buf, size_t count, off_t offset);
ssize_t (*libc_pwritev) (int fd, const struct iovec *iov, int iovcnt, off_t offset);
ssize_t (*libc_pwritev64) (int fd, const struct iovec *iov, int iovcnt, off_t offset);
ssize_t (*libc_readv) (int fd, const struct iovec * iov, int iovcnt);
ssize_t (*libc_send) (int s, const void *buf, size_t len, int flags);
ssize_t (*libc_sendto) (int s, const void *buf, size_t len, int flags,
const struct sockaddr * to, socklen_t tolen);
ssize_t (*libc_sendmsg) (int s, const struct msghdr * msg, int flags);
ssize_t (*libc_writev) (int fd, const struct iovec * iov, int iovcnt);
ssize_t (*libc_write) (int fd, const void *buf, size_t count);
ssize_t (*libc_sendfile) (int out_fd, int in_fd, off_t * offset, size_t count);
pid_t (*libc_wait) (int *status);
pid_t (*libc_waitpid) (pid_t pid, int *status, int options);
pid_t (*libc_wait3) (int *status, int options, struct rusage * rusage);
pid_t (*libc_wait4) (pid_t pid, int *status, int options, struct rusage * rusage);
int (*libc_shutdown) (int fd, int how);
int (*libc_pthread_create) (pthread_t * thread, const pthread_attr_t * attr, void *(*start_routine) (void *), void *arg);
void (*libc_pthread_exit) (void *value_ptr);
int (*libc_ppoll) (struct pollfd * fds, nfds_t nfds, const struct timespec * timeout, const sigset_t * sigmask);
int (*libc_poll) (struct pollfd * fds, nfds_t nfds, int timeout);
int (*libc_epoll_wait) (int epfd, struct epoll_event * events, int maxevents, int timeout);
int (*libc_epoll_pwait) (int epfd, struct epoll_event * events, int maxevents, int timeout, const sigset_t * sigmask);
int (*libc_epoll_create) (int size);
int (*libc_epoll_create1) (int flag);
int (*libc_epoll_ctl)(int epfd, int op, int fd, struct epoll_event *event);
int (*libc_select) (int nfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval * timeout);
int (*libc_pselect) (int nfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, const struct timespec * timeout, const sigset_t * sigmask);
int (*libc_fcntl)(int fd, int cmd, ...);
int (*libc_socketpair)(int domain, int type, int protocol, int sv[2]);
void (*libc_res_iclose)(void *statp, bool free_addr);
//void * (*libc_dlopen)(const char *filename, int flag);

#define QUOTEME(x) #x
#define find_func(x) (*((void**)&libc_##x) = dlsym (RTLD_NEXT, QUOTEME(x)));

extern "C" void __init_libc (void)
{
    find_func (res_nclose);
    if (libc_res_nclose==NULL) (*((void**)&libc_res_nclose)) = dlsym (RTLD_NEXT, "__res_nclose");

    find_func (res_close);
    if (libc_res_close==NULL) (*((void**)&libc_res_close)) = dlsym (RTLD_NEXT, "__res_close");

    find_func (res_iclose);
    if (libc_res_iclose==NULL) (*((void**)&libc_res_iclose)) = dlsym (RTLD_NEXT, "__res_iclose");

    find_func (dup2);
    find_func (dup);
    find_func (fork);
    find_func (vfork);
    find_func (execve);
    find_func (execvp);
    find_func (execv);
    find_func (wait);
    find_func (waitpid);
    find_func (wait3);
    find_func (wait4);
    find_func (exit);
    find_func (_exit);
    find_func (_Exit);
    find_func (accept);
    find_func (connect);
    find_func (close);
    find_func (bind);
    find_func (recv);
    find_func (recvfrom);
    find_func (recvmsg);
    find_func (read);
    find_func (open); 
    find_func (open64); 
    find_func (fopen); 
    find_func (fopen64); 
    find_func (fclose); 
    find_func (freopen); 
    find_func (freopen64); 
    find_func (creat); 
    find_func (creat64); 
    find_func (fdopen); 
    find_func (syslog); 
    find_func (vsyslog); 
    find_func (fwrite); 
    find_func (fwrite_unlocked); 
    find_func (pwrite); 
    find_func (pwrite64); 
    find_func (pwritev); 
    find_func (pwritev64); 
    find_func (readv);
    find_func (send);
    find_func (sendto);
    find_func (sendmsg);
    find_func (writev);
    find_func (write);
    find_func (sendfile);
    find_func (socket);
    find_func (shutdown);
    find_func (poll);
    find_func (ppoll);
    find_func (epoll_wait);
    find_func (epoll_pwait);
    find_func (epoll_create);
    find_func (epoll_create1);
    find_func (epoll_ctl);
    find_func (select);
    find_func (pselect);
    find_func (pthread_create);
    find_func (pthread_exit);
    find_func (fcntl);
    find_func (socketpair);
//    find_func (dlopen);
}
#endif
