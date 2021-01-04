#define _GNU_SOURCE
#include <stropts.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <ifaddrs.h>
#include <dirent.h>
#include <resolv.h>
#include <errno.h>
#include <sys/un.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <execinfo.h>
#include <net/if.h>
#include <time.h>
#include <sys/epoll.h>
#include <poll.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <aio.h>
#include <limits.h>

#define FD_PATH_SIZE 500 // bueffer size which will be used in 'getPathByFd()' (declared by wjhan)

#define SYSLOG 10
#define VLOG 11
int TRACE_LOG_MEDIA = VLOG;

#define PRINT_BUF_SIZE 2048
#define PRINT_LIMIT (p >= (((char*)printBuf)+PRINT_BUF_SIZE-1) ? 0 : (((char*)printBuf)+PRINT_BUF_SIZE-1-p))
#define isPrintableChar(x) (31<=(x) && (x) <127)
#define ERR_STRING (ret < 0 ? errString (save_errno) : "")
#define DEBUG_LOG_FD        524

char *__progname; // Program name, from crt0.
#define PROGNAME_LEN 16
#define HOSTNAME_LEN 64
static char progName[PROGNAME_LEN] = {"-"};
static char hname[HOSTNAME_LEN] = {"-"}; // bctak: 64 bytes should be sufficient to hold hostname
static struct timespec debug_ts1;
static struct timespec debug_ts2;

# define SET_START_TIME() { clock_gettime(CLOCK_REALTIME, &debug_ts1); }
# define SET_END_TIME() { clock_gettime(CLOCK_REALTIME, &debug_ts2); }
#define logerr(format,...) \
	( \
	  { \
	  char ____buf[PRINT_BUF_SIZE]; \
	  snprintf (____buf, PRINT_BUF_SIZE-1, __FILE__ ":%d " format, __LINE__, ##__VA_ARGS__); \
	  ____buf[PRINT_BUF_SIZE-1] = 0; \
	  syslog (LOG_INFO, "%s", ____buf); \
	  libc_write (2, ____buf, strlen(____buf)); \
	  })
#define debug_print(format,...) \
	( \
	  { \
	  char ____buf[PRINT_BUF_SIZE]; \
	  snprintf (____buf, PRINT_BUF_SIZE-1, format, ##__VA_ARGS__); \
	  ____buf[PRINT_BUF_SIZE-1] = 0; \
	  __debug_print (____buf); \
	  })


// Write the logs or send the logs
//  Before write(send)the logs, check the flag value for logging on/off.
# define dbg(format,...) ({ \
		long diffsec = debug_ts2.tv_sec - debug_ts1.tv_sec; \
		long diffns = debug_ts2.tv_nsec - debug_ts1.tv_nsec; \
		if (diffns<0) { \
		diffsec--; \
		diffns = 1000000000 - diffns; \
		} \
		if (TRACE_LOG_MEDIA == SYSLOG) { \
			if (strcmp(progName, "sshd")==0) {\
				return 0;\
			}\
		syslog(LOG_INFO,"%ld.%09ld %ld.%09ld %ld.%09ld %s %s %05d %lx " format, debug_ts1.tv_sec%10000, debug_ts1.tv_nsec, debug_ts2.tv_sec%10000, debug_ts2.tv_nsec, diffsec%10000, diffns, \
				progName, hname, getpid(), (unsigned long)pthread_self(), ##__VA_ARGS__); }\
				else { \
				debug_print ("%ld.%09ld %ld.%09ld %ld.%09ld %s %s %05d %lx " format, debug_ts1.tv_sec%10000, debug_ts1.tv_nsec, debug_ts2.tv_sec%10000, debug_ts2.tv_nsec, \
						diffsec%10000, diffns, \
						progName, hname, getpid(), (unsigned long)pthread_self(), ##__VA_ARGS__); }\
						})


/* For logging on/off  */
#define TURN_ON_LOGGING_PORT 20705  // The port number for receiving the request to turn on logging.
#define TURN_OFF_LOGGING_PORT 20803   // The port number for receiving the request to turn off logging.
static int FLAG_LOGGING = 1;  // Flag to decide to write and send logs. (1: on , 0 : off)
typedef void *(*thread_func) (void *data);

ssize_t (*libc_write)(int fd, const void *buf, size_t nbyte);
ssize_t (*libc_writev) (int fd, const struct iovec * iov, int iovcnt);
size_t (*libc_fwrite) (const void *ptr, size_t size, size_t nmemb, FILE *stream);
ssize_t (*libc_pwrite) (int fd, const void *buf, size_t count, off_t offset);
ssize_t (*libc_pwrite64) (int fd, const void *buf, size_t count, off64_t offset);
ssize_t (*libc_pwritev) (int fd, const struct iovec *iov, int iovcnt, off_t offset);
pid_t (*libc_fork) (void);
int (*libc_open) (const char *filename, int flags, ...);
int (*libc_open64) (const char *filename, int flags, ...);
int (*libc_close) (int fd);
int (*libc_fclose)(FILE *stream);
FILE *(*libc_fopen)(const char *pathname, const char *mode);
ssize_t (*libc_pwritev64)(int fd, const struct iovec * iov, int iovcnt, off64_t offset);
size_t (*libc_fwrite_unlocked)(const void *ptr, size_t size, size_t nmemb, FILE *stream);

//Thread function to handle request to turn off the logging
void * turn_off_logging (void *data)
{
	long lsock = (long)data;
	int sock = (int)lsock;

	while (1) {
		int clntSock;
		if ((clntSock = accept (sock, NULL, NULL)) >= 0) { // Connected to a new client socket
			/* Things to do when requested to turn off logging*/
			FLAG_LOGGING = 0;  // set flag to false(0) to turn off the logging
			char* message = "\nresult : turned off the logging\n\n"; // result message to send to client
			write(clntSock, message, strlen(message));  // send message to client
			//printf("FLAG_LOGGING : %d\n", FLAG_LOGGING);

			close (clntSock);  // Disconnect this client socket.
		}
	}

	return NULL;
}

//Thread function to handle request to turn on the logging
void * turn_on_logging (void *data)
{
	long lsock = (long)data;
	int sock = (int)lsock;

	while (1) {
		int clntSock;
		if ((clntSock = accept (sock, NULL, NULL)) >= 0) {
			/* Things to do when requested to turn on logging*/
			FLAG_LOGGING = 1;  // set flag to true(1) to turn on the logging
			char* message = "\nresult : turned on the logging\n\n";  // result message to send to client
			write(clntSock, message, strlen(message));  // send message to client
			//printf("FLAG_LOGGING : %d\n", FLAG_LOGGING);

			close (clntSock);
		}
	}

	return NULL;
}

// created by wjhan 18-03-07
// will retrun path which fd is linking.
char* getPathByFd(int cur_pid, int fd, char* path_buffer) {
	int read_size; // save string length of symlink path
	char sympath[FD_PATH_SIZE] ; // will save path of fd -> ex) '/proc/1010(pid)/fd/00'
	snprintf(sympath, sizeof(sympath), "/proc/%d/fd/%d", cur_pid, fd ); // create path with pid, fd
	read_size = readlink(sympath, path_buffer, FD_PATH_SIZE); // read real file path from fd path at 'path_buffer'
	path_buffer[read_size] = '\0'; // set end of string
	return path_buffer; // return real file path of fd
}


int isSocket (int fd)
{
	struct sockaddr_storage addr;
	socklen_t len = sizeof (addr);
	memset (&addr, 0, sizeof(addr));
	return getsockname (fd, (struct sockaddr *) &addr, &len) == 0;
}

#define DEFAULT_LOG_FD 2     // STDERR
int __debug_log_fd = DEFAULT_LOG_FD;
int __debug_log_fd_to_use = -1;
static pthread_mutex_t init_log_lock = PTHREAD_MUTEX_INITIALIZER;
static void init_file_sock_log (const char *file_sock_path)
{
	pthread_mutex_lock (&init_log_lock);
	if (__debug_log_fd != DEFAULT_LOG_FD)
		goto EXIT;

	__debug_log_fd = socket (AF_FILE, SOCK_DGRAM, 0);
	if (__debug_log_fd < 0) {
		logerr ("Cannot create socket.\n");
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
		int fd = dup (__debug_log_fd);
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
	if (connect (__debug_log_fd, (struct sockaddr *) &addr, sizeof (addr)) != 0) {
		logerr ("ERROR: cannot connect to %s\n", file_sock_path);
		libc_close (__debug_log_fd);
		__debug_log_fd = -1;
	}

EXIT:
	pthread_mutex_unlock (&init_log_lock);
}

void __debug_print (const char *buf)
{
	if (TRACE_LOG_MEDIA == SYSLOG) {
		syslog (LOG_INFO, "%s", buf);
		return;
	}

	int err = 0;
RETRY:
	if (TRACE_LOG_MEDIA == VLOG) {
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

	static const char *
errString (int err)
{
	switch (err) {
		case EPERM: return " EPERM";
		case ENOENT: return " ENOENT";
		case ESRCH: return " ESRCH";
		case EINTR: return " EINTR";;
		case EIO: return " EIO";
		case ENXIO: return " ENXIO";
		case E2BIG: return " E2BIG";;
		case ENOEXEC: return " ENOEXEC";
		case EBADF: return " EBADF";
		case ECHILD: return " ECHILD";
		case EAGAIN: return " EAGAIN";
		case ENOMEM: return " ENOMEM";
		case EACCES: return " EACCES";
		case EFAULT: return " EFAULT";
		case ENOTBLK: return " ENOTBLK";
		case EBUSY: return " EBUSY";
		case EEXIST: return " EEXIST";
		case EXDEV: return " EXDEV";
		case ENODEV: return " ENODEV";
		case ENOTDIR: return " ENOTDIR";
		case EISDIR: return " EISDIR";
		case EINVAL: return " EINVAL";
		case ENFILE: return " ENFILE";
		case EMFILE: return " EMFILE";
		case ENOTTY: return " ENOTTY";
		case ETXTBSY: return " ETXTBSY";
		case EFBIG: return " EFBIG";
		case ENOSPC: return " ENOSPC";
		case ESPIPE: return " ESPIPE";
		case EROFS: return " EROFS";
		case EMLINK: return " EMLINK";
		case EPIPE: return " EPIPE";
		case EDOM: return " EDOM";
		case ERANGE: return " ERANGE";
		case EDEADLK: return " EDEADLK";
		case ENAMETOOLONG: return " ENAMETOOLONG";
		case ENOLCK: return " ENOLCK";
		case ENOSYS: return " ENOSYS";
		case ENOTEMPTY: return " ENOTEMPTY";
		case ELOOP: return " ELOOP";
		case ENOMSG: return " ENOMSG";
		case EIDRM: return " EIDRM";
		case ECHRNG: return " ECHRNG";
		case EL2NSYNC: return " EL2NSYNC";
		case EL3HLT: return " EL3HLT";
		case EL3RST: return " EL3RST";
		case ELNRNG: return " ELNRNG";
		case EUNATCH: return " EUNATCH";
		case ENOCSI: return " ENOCSI";
		case EL2HLT: return " EL2HLT";
		case EBADE: return " EBADE";
		case EBADR: return " EBADR";
		case EXFULL: return " EXFULL";
		case ENOANO: return " ENOANO";
		case EBADRQC: return " EBADRQC";
		case EBADSLT: return " EBADSLT";
		case EBFONT: return " EBFONT";
		case ENOSTR: return " ENOSTR";
		case ENODATA: return " ENODATA";
		case ETIME: return " ETIME";
		case ENOSR: return " ENOSR";
		case ENONET: return " ENONET";
		case ENOPKG: return " ENOPKG";
		case EREMOTE: return " EREMOTE";
		case ENOLINK: return " ENOLINK";
		case EADV: return " EADV";
		case ESRMNT: return " ESRMNT";
		case ECOMM: return " ECOMM";
		case EPROTO: return " EPROTO";
		case EMULTIHOP: return " EMULTIHOP";
		case EDOTDOT: return " EDOTDOT";
		case EBADMSG: return " EBADMSG";
		case EOVERFLOW: return " EOVERFLOW";
		case ENOTUNIQ: return " ENOTUNIQ";
		case EBADFD: return " EBADFD";
		case EREMCHG: return " EREMCHG";
		case ELIBACC: return " ELIBACC";
		case ELIBBAD: return " ELIBBAD";
		case ELIBSCN: return " ELIBSCN";
		case ELIBMAX: return " ELIBMAX";
		case ELIBEXEC: return " ELIBEXEC";
		case EILSEQ: return " EILSEQ";
		case ERESTART: return " ERESTART";
		case ESTRPIPE: return " ESTRPIPE";
		case EUSERS: return " EUSERS";
		case ENOTSOCK: return " ENOTSOCK";
		case EDESTADDRREQ: return " EDESTADDRREQ";
		case EMSGSIZE: return " EMSGSIZE";
		case EPROTOTYPE: return " EPROTOTYPE";
		case ENOPROTOOPT: return " ENOPROTOOPT";
		case EPROTONOSUPPORT: return " EPROTONOSUPPORT";
		case ESOCKTNOSUPPORT: return " ESOCKTNOSUPPORT";
		case EOPNOTSUPP: return " EOPNOTSUPP";
		case EPFNOSUPPORT: return " EPFNOSUPPORT";
		case EAFNOSUPPORT: return " EAFNOSUPPORT";
		case EADDRINUSE: return " EADDRINUSE";
		case EADDRNOTAVAIL: return " EADDRNOTAVAIL";
		case ENETDOWN: return " ENETDOWN";
		case ENETUNREACH: return " ENETUNREACH";
		case ENETRESET: return " ENETRESET";
		case ECONNABORTED: return " ECONNABORTED";
		case ECONNRESET: return " ECONNRESET";
		case ENOBUFS: return " ENOBUFS";
		case EISCONN: return " EISCONN";
		case ENOTCONN: return " ENOTCONN";
		case ESHUTDOWN: return " ESHUTDOWN";
		case ETOOMANYREFS: return " ETOOMANYREFS";
		case ETIMEDOUT: return " ETIMEDOUT";
		case ECONNREFUSED: return " ECONNREFUSED";
		case EHOSTDOWN: return " EHOSTDOWN";
		case EHOSTUNREACH: return " EHOSTUNREACH";
		case EALREADY: return " EALREADY";
		case EINPROGRESS: return " EINPROGRESS";
		case ESTALE: return " ESTALE";
		case EUCLEAN: return " EUCLEAN";
		case ENOTNAM: return " ENOTNAM";
		case ENAVAIL: return " ENAVAIL";
		case EISNAM: return " EISNAM";
		case EREMOTEIO: return " EREMOTEIO";
		case EDQUOT: return " EDQUOT";
		case ENOMEDIUM: return " ENOMEDIUM";
		case EMEDIUMTYPE: return " EMEDIUMTYPE";
		case ECANCELED: return " ECANCELED";
		case ENOKEY: return " ENOKEY";
		case EKEYEXPIRED: return " EKEYEXPIRED";
		case EKEYREVOKED: return " EKEYREVOKED";
		case EKEYREJECTED: return " EKEYREJECTED";
		default: return " err-unknown";
	}
}

static inline const char *sockTypeString (int sockType)
{
	switch (sockType) {
		case SOCK_STREAM: return "SOCK_STREAM";
		case SOCK_DGRAM: return "SOCK_DGRAM";
		case SOCK_RAW: return "SOCK_RAW";
		case SOCK_RDM: return "SOCK_RDM";
		case SOCK_SEQPACKET: return "SOCK_SEQPACKET";
		case SOCK_PACKET: return "SOCK_PACKET";
		default: return "SOCK_unknown";
	}
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

int printData (char *p, int printLimit, char *buf, int len)
{
	int n=0, m=0;
	if (printLimit <= 0 || len <=0) return 0;

	for (n = 0; n < len; n++) {
		if (m+2 >= printLimit) break;

		if (buf[n] == '\r') {
			p[m] = '\\';
			p[m+1] = 'r';
			m += 2;
		}
		else if (buf[n] == '\n') {
			p[m] = '\\';
			p[m+1] = 'n';
			m += 2;
		}
		else if (buf[n] == '\t') {
			p[m] = '\\';
			p[m+1] = 't';
			m += 2;
		}
		else {
			p[m] = isPrintableChar (buf[n]) ? buf[n] : '.';
			m++;
		}
	}

	p[m] = 0;
	return m;
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

	static void
init_progname ()
{
	int i;
	for (i = 0; i < PROGNAME_LEN - 2; i++) {
		if (__progname[i] == 0) break;
		progName[i] = __progname[i];
	}

	progName[PROGNAME_LEN - 1] = 0;

	// TODO: check if platform is LINUX. AIX doesn't have /proc.
	if (!strncmp("python", progName, 6)) {
		int fd;
		char fname[PATH_MAX];
		char line[PATH_MAX];
		char *newprog;
		sprintf(fname,"/proc/%d/cmdline",getpid());
		fd = libc_open(fname,O_RDONLY);
		if (fd>=3) {
			read(fd, line, PATH_MAX);
			libc_close(fd);
			while (line[i]!='\0') i++;
			newprog = line + i + 1;
			newprog = newprog + strlen(newprog) - 1;
			while (newprog[0]!='/') newprog--;
			newprog++;
			snprintf(progName,PROGNAME_LEN,"%s",newprog);
		}
	}

	if (gethostname(hname, HOSTNAME_LEN)<0)
		strncpy(hname, "unknown", HOSTNAME_LEN);
};

static int findUnusedFD (int fd)
{
	int i;
	for (i = 0; i < 500; i++) {
		if (fcntl (fd, F_GETFD) == -1) return fd;
		fd++;
	}

	return -1;        // Did not find an unused fd.
}

/*
mutex in shared memory for sync
not used.. But if need to use mutex between each programs, use it.
*/
#define retm_if(expr, val, msg) do { \
	if (expr) \
	{ \
		printf("%s\n", (msg)); \
		return (val); \
	} \
} while(0)

#define retv_if(expr, val) do { \
	if (expr) \
	{ \
		return (val); \
	} \
} while(0)

#define rete_if(expr, val, msg) do { \
	if (expr) \
	{ \
		printf("%s, errno : %d, errstr : %s\n", msg, errno, strerror(errno)); \
		return (val); \
	} \
} while(0)


#define SHM_NAME "shm_lock"

typedef struct shm_struct {
	pthread_mutex_t mtx;
	int idx;
} shm_struct_t;

//Constructor that will be invoked before main function
//Modified by wjhan ...2018.02.08
int constructor_called = 0; // Flag that tells if the constructor has been called yet
void __attribute__((constructor)) bluecoat_init(void)
{
	if (!constructor_called) { // To ensure constructor is only called once
		constructor_called = 1;
		// printf("Constructor Invoked!\n");
		// Run listener thread that handles incoming socket request to a specific port num

		//syslog(LOG_INFO, "bluecoat_init() pid:%d LD_PRELOAD:%s \n", getpid(), getenv("LD_PRELOAD"));
		libc_fwrite_unlocked = dlsym(RTLD_NEXT, "fwrite_unlocked");
		libc_open = dlsym(RTLD_NEXT, "open");
		libc_open64 = dlsym(RTLD_NEXT, "open64");
		libc_fopen = dlsym(RTLD_NEXT, "fopen");
		libc_write = dlsym(RTLD_NEXT, "write");
		libc_writev = dlsym(RTLD_NEXT, "writev");
		libc_fwrite = dlsym(RTLD_NEXT, "fwrite");
		libc_pwrite = dlsym(RTLD_NEXT, "pwrite");
		libc_pwrite64 = dlsym(RTLD_NEXT, "pwrite64");
		libc_pwritev = dlsym(RTLD_NEXT, "pwritev");
		libc_pwritev64 = dlsym(RTLD_NEXT, "pwritev64");
	//	libc_pwritev64 = dlsym(RTLD_NEXT, "pwritev64");
		libc_close = dlsym(RTLD_NEXT, "close");
		libc_fclose = dlsym(RTLD_NEXT, "fclose");
	//	libc_fwrite_unlocked = dlsym(RTLD_NEXT, "fwrite_unlocked");
		init_progname();

		setlogmask (LOG_UPTO (LOG_INFO));
		openlog ("bluecoat", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

		__debug_log_fd_to_use = findUnusedFD (DEBUG_LOG_FD);
	}
}

// Destructor that will be invoke after main() function
// Modified by wjhan ... 2018.02.08
void __attribute__((destructor)) bluecoat_fini(void)
{
	// printf("Destructor Invoked!");

	//dbg("bluecoat_fini() pid:%d LD_PRELOAD:%s \n", getpid(), getenv("LD_PRELOAD"));
	//munmap(shm, sizeof(shm_struct_t)); // destroy shared memory
}


// syscalls below

int close(int fd) {
	bluecoat_init();

	struct stat sb;
	if (fstat(fd, &sb) == -1)
		return libc_close(fd);

	if ((sb.st_mode & S_IFMT) == S_IFSOCK)
		return libc_close(fd);


	char path_buffer[FD_PATH_SIZE]; // for PATH of FD (to the log)
	char *path_buffer_ptr = getPathByFd(getpid(), fd, path_buffer);

	SET_START_TIME();
	const int ret = libc_close(fd);
	const int save_errno = errno;
	SET_END_TIME();



	char printBuf[PRINT_BUF_SIZE];
	char *p = printBuf;
	p += snprintf (p, PRINT_LIMIT, "close %d %d ", ret, fd);
	// Write path that `fd` is linking
	p += snprintf (p, PRINT_LIMIT, " %s ", path_buffer_ptr);

	errno = save_errno;
	return ret;

}
