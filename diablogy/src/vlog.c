#include <sys/un.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
/* for POSIX threads */
#include <pthread.h>
#include "utils.h"
/* for sync of bluecoat */
#include <sys/shm.h>
#include <sys/mman.h>
#include <errno.h>
/* cassandra c driver */
#include <cassandra.h>

#define SOCK_FILE "/tmp/vpath.log.sock"
#define LOG_FILE "/tmp/vpath.log"
#define RECOVER_PORT    13081
#define TRUNCATE_PORT   13082
#define LOG_SIZE        100000000
#define MAX_LOG_SIZE 1000

typedef void *(*thread_func) (void *data);

inline void ignore_ret_val (int x)
{
	(void) x;
}

/*
Shared mutex for sync of bluecoat events
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
	pthread_mutex_t mutex;
	int idx;
}shm_struct_t;

static shm_struct_t *shm;
static pthread_mutexattr_t mtx_attr;
int check_logfile = 0;


static void __destroy_shared_memory(void)
{
	pthread_mutex_destroy(&shm->mutex);
	pthread_mutexattr_destroy(&mtx_attr);

	munmap(shm, sizeof(shm_struct_t));
}
// shared mutex end

/* cassandra driver library variables */
CassCluster* cluster;
CassSession* session;
CassFuture* connect_future;
CassError rc;
CassFuture* query_future;
char* cassandra_serv_ip = "155.230.91.227"; // contact points : cassandra server ip
char name_of_db_table[MAX_LOG_SIZE]; // the name of key-value-store that will be TABLE NAME. (mongodb.write)

static int FLAG_FILE_WRITE = 1;  // Flag to decide to write log to file.
static volatile int logfd = 2;
void init_logfd()
{
	logfd = open (LOG_FILE, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (logfd < 0) perror ("Cannot create " LOG_FILE);
}

void * delete_preload (void *data)
{
	long lsock = (long)data;
	int sock = (int)lsock;

	while (1) {
		int clntSock;
		if ((clntSock = accept (sock, NULL, NULL)) >= 0) {
			unlink ("/etc/ld.so.preload");
			ignore_ret_val (write(clntSock, "/etc/ld.so.preload removed\n", 27));
			close (clntSock);
		}
	}

	return NULL;
}

void * truncate_log (void *data)
{
	long lsock = (long)data;
	int sock = (int)lsock;

	while (1) {
		int clntSock;
		if ((clntSock = accept (sock, NULL, NULL)) >= 0) {
			close (logfd);
			ignore_ret_val(truncate (LOG_FILE, 0));
			init_logfd();
			ignore_ret_val(write (clntSock, "/tmp/vpath.log truncated\n", 23));
			close (clntSock);
		}
	}

	return NULL;
}

static void init_listener (thread_func func, int port)
{
	int sock;
	struct sockaddr_in addr;
	if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		perror ("socket");

	memset (&addr, 0, sizeof (addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl (INADDR_ANY);
	addr.sin_port = htons (port);

	int yes = 1;
	if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int)) == -1) {
		perror ("setsockopt");
	}

	if (bind (sock, (struct sockaddr *) &addr, sizeof (addr)) < 0)
		perror ("bind");

	if (listen (sock, 10) < 0)
		perror ("listen");

	long lsock = sock;
	pthread_t threadID;
	if (pthread_create (&threadID, NULL, func, (void*)lsock) != 0) {
		perror ("pthread_create");
	}
}

/* for cassandra driver */

// remove that interfere with the query string
void eliminate_ch(char *str)
{
	int i=0;
	int str_size = strlen(str);
	for(i=0; i<str_size; i++) {
		if(str[i]=='\'' || str[i]=='\n' || str[i]=='\"' )
			str[i]= '.';
	}
}


int EndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}




#define COL_NUM 17
// Split the log with a space character " ".
void split_log(char* log, char log_split_list[][MAX_LOG_SIZE]) {
	char log_cp[MAX_LOG_SIZE*10];
	char * split;
	int count = 0;

	strncpy(log_cp, log, MAX_LOG_SIZE*10 - 1);  // copy string
	log_cp[MAX_LOG_SIZE*10 - 1] = 0;

	for(int i = 0; i < COL_NUM; i++) memset(log_split_list[i], 0, MAX_LOG_SIZE); // reset log_split_list

	eliminate_ch(log_cp);

	split = strtok(log_cp, " ");
	while (split != NULL)
	{
		count ++;
		if(count >= 11 && ( strcmp(log_split_list[11], "NON_SOCKET")==0 || strcmp(log_split_list[9], "-1")==0 )) {
			if(count >= 13) {
				strcat(log_split_list[13], split);
				strcat(log_split_list[13], " ");
			}else {
				strcpy(log_split_list[count], split);
			}

		}else {

			if(count >= 16) {
				strcat(log_split_list[16], split);
				strcat(log_split_list[16], " ");
			}else {
				strcpy(log_split_list[count], split);
			}
		}

		split = strtok(NULL, " ");
	}// end of while

}


int event_count = 0;
// Create  an 'insert' query statement to store the log in Cassandra.
char* get_insert_query_string(char* log, char* insert_query_string) {

	char split_list[COL_NUM][MAX_LOG_SIZE];
	char pre[MAX_LOG_SIZE] = "INSERT INTO "; // + "TABLE_NAME"
	// cqlsh> CREATE TABLE beta.openstack (id int PRIMARY KEY, start_name double, end_time double, turnaround_time double, proc_name varchar, host_name varchar, proc_id varchar, thread_id varchar, sys_call varchar, return_byte int, fd int, pipe_val varchar, valid varchar);

	char col_name_string[MAX_LOG_SIZE] = "(id, start_time, end_time, turnaround_time, proc_name, host_name, proc_id, thread_id, sys_call, return_byte, fd, pipe_val, contents, valid";
		// add current_time now() to cassandra query
	char mid[MAX_LOG_SIZE]  = ") VALUES (";
	char value_string[MAX_LOG_SIZE*COL_NUM] = "";
	char post[MAX_LOG_SIZE]  = "A');";
	char *p = value_string;

	strcat(pre, name_of_db_table); // "insert into TABLE_NAME"
	strcpy(insert_query_string, ""); // reset buffer
	split_log(log, split_list); // split log string and save it to 'split_list'

	// int trans_id = matching_transaction(split_list);

	p += snprintf(p, MAX_LOG_SIZE, "%d, ", event_count++);

	for(int i = 1; i <= 12; i++) {
		if(i==0 || i==1 || i==2 || i==3 || i==9 || i==10)	// number type values
			p += snprintf(p, MAX_LOG_SIZE, "%s", split_list[i]);
		else	// string type values
			p += snprintf(p, MAX_LOG_SIZE, "'%s'", split_list[i]);
		if(i<COL_NUM-1)
		    p += snprintf(p, MAX_LOG_SIZE, ", ");
	}

	if(EndsWith(split_list[11], ".log") == 1 || strcmp(split_list[8], "close") == 0)
		check_logfile = 1;
        else
		check_logfile = 0;
	strcat(insert_query_string, pre);
	strcat(insert_query_string, col_name_string);
	strcat(insert_query_string, mid);
	strcat(insert_query_string, value_string);
	if (insert_query_string[strlen(insert_query_string)-1] != '\'')
        strcat(insert_query_string, "'"); // if there are no ' because of overflow or any reason
	strcat(insert_query_string, post);
	return insert_query_string;
}


/* Setup and Connect Cassandra*/
void setup_cassandra_driver() {
	/* Setup and connect to cluster */
	cluster = cass_cluster_new();
	session = cass_session_new();

	/* Add contact points */
	cass_cluster_set_contact_points(cluster, cassandra_serv_ip);

	/* Provide the cluster object as configuration to connect the session */
	connect_future = cass_session_connect(session, cluster);

	/* This operation will block until the result is ready */
	rc = cass_future_error_code(connect_future);
	if (rc == 0)
		printf("Connection Successfull\n");
	else
		printf("Connect result: ERROR  %s\n", cass_error_desc(rc));

}


/* Run queries...insert */
void execute_cassandra_insert(char* query_string) {

	//const char * err_str ;
	/* Create a statement with zero parameters */
	CassStatement* statement = cass_statement_new(query_string, 0);
	query_future = cass_session_execute(session, statement);

	/* Statement objects can be freed immediately after being executed */
	cass_statement_free(statement);

	/* This will block until the query has finished */
	rc = cass_future_error_code(query_future);
	//err_str = cass_error_desc(rc);
	
	//Just ignoring the error at the moment but, needs to fix it later
	//if(rc == 0) {
		printf("\n** Query result**: %s\n", "Successfull");
		printf("(INFO) query = %s\n", query_string); // this will be included above if statment.
	//}
}


/* Terminate Cassandra Driver */
void terminate_cassandra_driver() {
	cass_future_free(query_future);
	cass_future_free(connect_future);
	cass_session_free(session);
	cass_cluster_free(cluster);
}


static void terminate_handler(int signo) {
	//terminate_cassandra_driver(); /* Terminate Cassandra Driver */

        // Wait for thread 'interface_control()'
        // pthread_join(thread_inter, (void **)&status );
	__destroy_shared_memory();
        printf ("\nThread end (exit status %d ) \n", signo);
	exit(0);
}


int main (int argc, char **argv)
{
	int sock;
	struct sockaddr_un servaddr; /* address of server */
	long maxLogSize = LOG_SIZE;
	//pthread_t thread_inter ; //  variable to run thread for 'interface_control()'
	int status; // variable to save the exit status of the thread
	char query[MAX_LOG_SIZE*COL_NUM] = ""; // buffer to store cassandra query string.

	if (getuid() != 0) {
		fprintf (stderr, "This program must be run as `root' so that, if necessary, \nit has the permission to remove /etc/ld.so.preload to help recovery.\n");
		exit (1);
	}

	//    if (argc==2) maxLogSize = atol (argv[1]);

	if (argc==2){
		strcpy(name_of_db_table, argv[1]);
	}else {
		strcpy(name_of_db_table, "beta.fullstack");
	}


	if ((sock = socket (AF_UNIX, SOCK_DGRAM, 0)) < 0) {
		perror ("socket");
		exit (2);
	}

	if (unlink (SOCK_FILE) && !access (SOCK_FILE, F_OK))
		perror ("Cannot remove " SOCK_FILE);

	memset (&servaddr, 0, sizeof (servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy (servaddr.sun_path, SOCK_FILE);

	if (bind (sock, (struct sockaddr *) &servaddr, sizeof (servaddr)) < 0 || chmod(SOCK_FILE, 0666) < 0) {
		close (sock);
		perror ("bind");
		exit (3);
	}

	/*char lbuf[1024];
	  printLocalAddr (lbuf, sock);
	  printf ("bind: `%s'\n", lbuf);
	 */

	init_logfd();
	init_listener(delete_preload, RECOVER_PORT);
	init_listener(truncate_log, TRUNCATE_PORT);

	printf ("Log file: %s\n", LOG_FILE);
	printf ("Maximum log file size: %ld MB\n", maxLogSize/1048576);
	printf ("To delete /etc/ld.so.preload: telnet localhost %d\n", RECOVER_PORT);
	printf ("To truncate %s: telnet localhost %d\n", LOG_FILE, TRUNCATE_PORT);
	printf ("Press Ctrl+C to terminate program\n");

	const int N = 1048576;
	char * buf = malloc (N);
	if (buf==NULL) {
		perror ("malloc");
		exit (2);
	}

	// run thread for interface_control()
	// 2018.02.08 Don't use this thread
	//if ( pthread_create(&thread_inter, NULL, interface_control, NULL) < 0) {
	//    perror("thread create error (interface_control)");
	//    exit(0);
	//}

	setup_cassandra_driver(); /* Setup and Connect Cassandra */


	signal(SIGINT, terminate_handler);

	int count = 0;
	while (1) {
		memset(buf, 0, N);
		int n = read (sock, buf, N);
		if (n < 0) {
			perror ("read");
			return 1;
		}

		buf[n++] = '\n';

		count++;
		if (count>=1024) {
			long size = lseek(logfd, 0, SEEK_END);
			if (size >= maxLogSize) {
				fprintf (stderr, "Truncate log file as the current size (%ld) exceeded the maximum log file size (%ld bytes)\n", size, maxLogSize);
				close (logfd);
				init_logfd ();
			}

			count = 0;
		}

		const char *p = buf;

		/* Write to cassandra */

		get_insert_query_string(buf, query); /* Create query */
		if (check_logfile == 1) {
			execute_cassandra_insert(query); /* Run querry */
		}
			// Check flag(defult:TRUE(1)) and Write the logs(buf) to file descriptor (logfd : LOG_FILE = /tmp/vpath.log)
			while ( FLAG_FILE_WRITE && n > 0) {
				int r = write (2, p, n);
				if (r < 0) {
					perror ("write");
					return 2;
				}
				p += r;
				n -= r;
			}
		//}
	}

	// terminate_cassandra_driver(); /* Terminate Cassandra Driver */


	// Wait for thread 'interface_control()'
	// pthread_join(thread_inter, (void **)&status );
	printf("Thread end (exit status %d ) \n", status);
	return 0;
}

