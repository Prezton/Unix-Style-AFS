#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
// #include <messages_info.h>

// The following line declares a function pointer with the same prototype as the open function.  
int (*orig_open)(const char *pathname, int flags, ...);  // mode_t mode is needed when flags includes O_CREAT
int (*orig_close)(int fd);
ssize_t (*orig_read)(int fd, void *buf, size_t count);
ssize_t (*orig_write)(int fd, const void *buf, size_t count);
off_t (*orig_lseek)(int fd, off_t offset, int whence);
int (*orig_stat)(const char *pathname, struct stat *statbuf);
int (*orig_xstat)(int ver, const char * path, struct stat * stat_buf);
int (*orig_unlink)(const char *pathname);
ssize_t (*orig_getdirentries)(int fd, char *buf, size_t nbytes, off_t *basep);
struct dirtreenode* (*orig_getdirtree)( const char *path );
void (*orig_freedirtree)( struct dirtreenode* dt );

#define MAXMSGLEN 200

char *serverport;
unsigned short port;
int sockfd;
char *serverip;

// Sending and receiving message from server
void connect_message(char *buf) {

	int rv = send(sockfd, buf, strlen(buf), 0);
	if (rv < 0) {
		fprintf(stderr, "error in sending\n");
	}
	// orig_close(sockfd);
}

// Sending and receiving message from server
char *send_message(char *buf, int total_length) {

	// send out parameters packed in buf
	int sv = send(sockfd, (buf), total_length, 0);
	fprintf(stderr, "inside send_message, sv == %d, total_length == %d\n", sv, total_length);
	if (sv <= 0) {
		fprintf(stderr, "error when client send out request!! sv == 0\n");
	}
	// receive from the server of return values
	// message protocol: total_return_size + return message
	char *size_pointer = malloc(sizeof(int));
	char *received_size = malloc(sizeof(int));
	int bytes_received = 0;
	int rv;
	while ( (rv=recv(sockfd, size_pointer, sizeof(int) - bytes_received, 0)) > 0) {
		// check validity

		// create message from buf
		memcpy(received_size + bytes_received, size_pointer, rv);
		bytes_received += rv;
		// enough bytes have been received
		if (bytes_received >= sizeof(int)) {
			break;
		}
	}
	if (rv<=0) {
		fprintf(stderr, "client receive response error 1\n");
	}
	int reply_size = *((int *)received_size);	

	char *reply_message = malloc(reply_size);
	char *temp = malloc(reply_size);
	bytes_received = 0;

	while ( (rv=recv(sockfd, temp, reply_size - bytes_received, 0)) > 0) {
		memcpy(reply_message + bytes_received, temp, rv);
		bytes_received += rv;
		if (bytes_received >= reply_size) {
			break;
		}
	}
	if (rv<=0) {
		fprintf(stderr, "client receive response error 2\n");
	}
	return reply_message;
	// orig_close(sockfd);
}

// This is our replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}
	// we just print a message, then call through to the original open function (from libc)
	// fprintf(stderr, "mylib: open called for path %s\n", pathname);
	// assign opcode of "open" as 66
	int opcode = 66;
	int starter = 0;

	// overall size of message, protocol:
	// itself + opcode + flag + pathname size + mode_t + pathname
	int total_length = 4 * sizeof(int) + strlen(pathname) + sizeof(mode_t);
    char *message = malloc(total_length);

	memcpy(message + starter, &total_length, sizeof(int));
	starter += sizeof(int);
	// set opcode
	memcpy(message + starter, &opcode, sizeof(int));
	starter += sizeof(int);
	// set flag
	memcpy(message + starter, &flags, sizeof(int));
	starter += sizeof(int);

	// set header (size) of pathname
	int pathname_size = strlen(pathname);
	memcpy(message + starter, &pathname_size, sizeof(int));
	starter += sizeof(int);

	// set mode_t
	memcpy(message + starter, &m, sizeof(mode_t));
	starter += sizeof(mode_t);
    // send message to server, indicating type of operation

	// set pathname
	memcpy(message + starter, pathname, pathname_size);
	starter += pathname_size;
	
	fprintf(stderr, "client called open: pathname %s, flags %d, mode %d, total length: %d\n", pathname, flags, m, total_length);
	char *received_message = send_message(message, total_length);

	int received_fd = *((int *)received_message);
	int received_errno = *((int *)received_message + 1);
	fprintf(stderr, "client received from open call: fd %d, errno %d !!\n", received_fd, received_errno);
	// return orig_open(pathname, flags, m);
	if (received_errno != 0 || received_fd < 0) {
		errno = received_errno;
	}
	return received_fd;
}

int close(int fd) {
	int opcode = 67;
	int total_length = 3 * sizeof(int);
	int starter = 0;
	// message protocol:
	// total size + opcode + fd
	char *message = malloc(3 * sizeof(int));

	memcpy(message + starter, &total_length, sizeof(int));
	starter += sizeof(int);
	memcpy(message + starter, &opcode, sizeof(int));
	starter += sizeof(int);
	memcpy(message + starter, &fd, sizeof(int));
	starter += sizeof(int);

	fprintf(stderr, "client called close: length %d, fd %d\n", total_length, fd);

	char *received_message = send_message(message, total_length);
	int result = *received_message;
	int received_errno = *(received_message + 1);
	fprintf(stderr, "client received from close call: result %d, errno %d !!\n", result, received_errno);

	if (result != 0 || received_errno != 0) {
		errno = received_errno;
	}
	return result;


	// char *message = "close\n";
	// connect_message(message);
	// return orig_close(fd);
}

ssize_t read(int fd, void *buf, size_t count) {


	// assign opcode of "read" as 68
	int opcode = 68;
	int starter = 0;

	// overall size of message, protocol:
	// itself + opcode + fd + count

	// dont send out strlen(buf)!!!!
	int total_length = 3 * sizeof(int) + sizeof(size_t);
    char *message = malloc(total_length);

	memcpy(message + starter, &total_length, sizeof(int));
	starter += sizeof(int);
	// set opcode
	memcpy(message + starter, &opcode, sizeof(int));
	starter += sizeof(int);
	// set fd
	memcpy(message + starter, &fd, sizeof(int));
	starter += sizeof(int);

	// set count
	memcpy(message + starter, &count, sizeof(mode_t));
	starter += sizeof(size_t);


    // send message to server, indicating type of operation

	fprintf(stderr, "client called read: fd %d, count %lu, total length %d\n", fd, count, total_length);
	char *received_message = send_message(message, total_length);
	int received_errno = *((int *)received_message);
	ssize_t received_num;
	memcpy(&received_num, received_message + sizeof(int), sizeof(ssize_t));
	memcpy(buf, received_message + sizeof(int) + sizeof(ssize_t), count);
	fprintf(stderr, "client received from read call: bytes read %lu, errno %d, contents: %s !!\n", received_num, received_errno, (char *)buf);
	
	if (received_num == -1) {
		errno = received_errno;
	}
	return received_num;
}

ssize_t write (int fd, const void *buf, size_t count) {
	// assign opcode of "write" as 69
	int opcode = 69;
	int starter = 0;

	// overall size of message, protocol:
	// itself + opcode + fd + count + buf

	// dont send out strlen(buf)!!!!
	int total_length = 3 * sizeof(int) + sizeof(size_t) + count;
    char *message = malloc(total_length);

	memcpy(message + starter, &total_length, sizeof(int));
	starter += sizeof(int);
	// set opcode
	memcpy(message + starter, &opcode, sizeof(int));
	starter += sizeof(int);
	// set fd
	memcpy(message + starter, &fd, sizeof(int));
	starter += sizeof(int);

	// set count
	memcpy(message + starter, &count, sizeof(mode_t));
	starter += sizeof(size_t);

	// set buf
	memcpy(message + starter, buf, count);
	starter += count;

    // send message to server, indicating type of operation

	fprintf(stderr, "client called write: fd %d, count %lu, total length %d\n", fd, count, total_length);
	char *received_message = send_message(message, total_length);

	// may need to change to int * here!!!
	int received_errno = *((int *)received_message);

	ssize_t received_num;
	memcpy(&received_num, received_message + sizeof(int), sizeof(ssize_t));

	if (received_num == -1 || received_errno != 0) {
		errno = received_errno;
	}
	fprintf(stderr, "client received from write call: bytes written %lu, errno %d !!\n", received_num, received_errno);

	return received_num;

}

off_t lseek(int fd, off_t offset, int whence) {
	// assign opcode of "lseek" as 70
	int opcode = 70;

	// itself + opcode + fd + whence + offset

	int total_length = 4 * sizeof(int) + sizeof(off_t);
	char *message = malloc(total_length);
	int starter = 0;

	memcpy(message + starter, &total_length, sizeof(int));
	starter += sizeof(int);
	memcpy(message + starter, &opcode, sizeof(int));
	starter += sizeof(int);
	memcpy(message + starter, &fd, sizeof(int));
	starter += sizeof(int);
	memcpy(message + starter, &whence, sizeof(int));
	starter += sizeof(int);
	memcpy(message + starter, &offset, sizeof(off_t));
	starter += sizeof(off_t);

	fprintf(stderr, "client called lseek: fd %d, whence %d, offset %ld, total_length %d\n", fd, whence, offset, total_length);
	// errno + result
	char *received_message = send_message(message, total_length);
	int received_errno = *((int *)received_message);

	off_t received_result;
	memcpy(&received_result, received_message + sizeof(int), sizeof(off_t));
	if (received_errno != 0) {
		errno = received_errno;
	}
	fprintf(stderr, "client received from lseek call: errno %d, result %ld\n", received_errno, received_result);

	return received_result;
}

// int stat (const char *pathname, struct stat *statbuf) {
// 	char *message = "stat\n";
// 	connect_message(message);
// 	return orig_stat(pathname, statbuf);
// }

int __xstat(int ver, const char * path, struct stat * stat_buf) {
	// assign opcode of "xstat" as 71
	int opcode = 71;
	int starter = 0;

	// overall size of message, protocol:
	// itself (int) + opcode (int) + ver (int) + strlen(path) (int) + path

	int total_length = 4 * sizeof(int) + strlen(path);
	
	char *message = malloc(total_length);

	memcpy(message + starter, &total_length, sizeof(int));
	starter += sizeof(int);
	memcpy(message + starter, &opcode, sizeof(int));
	starter += sizeof(int);
	memcpy(message + starter, &ver, sizeof(int));
	starter += sizeof(int);
	int path_size = strlen(path);
	memcpy(message + starter, &path_size, sizeof(int));
	starter += sizeof(int);
	memcpy(message + starter, path, path_size);
	starter += path_size;

	fprintf(stderr, "client called __xstat: ver %d, path_size %d, path %s total length %d\n", ver, path_size, path, total_length);
	// send out message
	char *received_message = send_message(message, total_length);
	int received_errno = *((int *)received_message);
	int received_result = *((int *)received_message + 1);
	memcpy(stat_buf, received_message + 2 * sizeof(int), sizeof(struct stat));
	if (received_result == -1) {
		errno = received_errno;
	}
	fprintf(stderr, "client received from __xstat call: errno %d, result %d, stat_buf %s\n", received_errno, received_result, stat_buf);

	return received_result;
}

int unlink (const char *pathname) {

	int opcode = 72;
	int starter = 0;

	// overall size of message, protocol:
	// itself + opcode + path size + pathname
	int total_length = 3 * sizeof(int) + strlen(pathname);
    char *message = malloc(total_length);

	memcpy(message + starter, &total_length, sizeof(int));
	starter += sizeof(int);
	// set opcode
	memcpy(message + starter, &opcode, sizeof(int));
	starter += sizeof(int);
	// set pathname size
	int path_size = strlen(pathname);
	memcpy(message + starter, &path_size, sizeof(int));
	starter += sizeof(int);
	memcpy(message + starter, pathname, path_size);

	fprintf(stderr, "client called unlink: path size %d, pathname %s, total_length %d\n", path_size, pathname, total_length);
	char *received_message = send_message(message, total_length);
	int received_errno = *((int *)received_message);
	int received_result = *((int *)received_message + 1);
	if (received_result == -1) {
		errno = received_errno;
	}
	fprintf(stderr, "client received from unlink call: errno %d, result %d\n", received_errno, received_result);

	return received_result;
}
ssize_t getdirentries (int fd, char *buf, size_t nbytes, off_t *basep) {
	// assign opcode of "open" as 73
	int opcode = 73;
	int starter = 0;

	// overall size of message, protocol:
	// itself + opcode + fd + nbytes + basep
	int total_length = 3 * sizeof(int) + sizeof(size_t) + sizeof(off_t);
    char *message = malloc(total_length);

	memcpy(message + starter, &total_length, sizeof(int));
	starter += sizeof(int);
	// set opcode
	memcpy(message + starter, &opcode, sizeof(int));
	starter += sizeof(int);
	// set flag
	memcpy(message + starter, &fd, sizeof(int));
	starter += sizeof(int);

	memcpy(message + starter, &nbytes, sizeof(size_t));
	starter += sizeof(size_t);

	memcpy(message + starter, basep, sizeof(off_t));
	starter += sizeof(off_t);
	fprintf(stderr, "client called getdirentries: fd %d, nbytes %ld, basep %ld\n", fd, nbytes, *basep);

	char *received_message = send_message(message, total_length);
	int received_errno = *((int *)received_message);
	ssize_t received_result;

	memcpy(&received_result, received_message + sizeof(int), sizeof(ssize_t));

	if (received_result != -1) {
		memcpy(buf, received_message + sizeof(int) + sizeof(ssize_t), received_result);
	}
	fprintf(stderr, "client received from getdirentries call: errno %d, bytes_read %ld\n", received_errno, received_result);

	if (received_result == -1) {
		errno = received_errno;
	}

	return received_result;
}

struct dirtreenode* getdirtree(const char *path ) {
	char *message = "getdirtree\n";
	connect_message(message);
	return orig_getdirtree(path);
}

void freedirtree( struct dirtreenode* dt ) {
	char *message = "freedirtree\n";
	connect_message(message);
	return orig_freedirtree(dt);
}




// This function is automatically called when program is started
void _init(void) {
	// set function pointer orig_open to point to the original open function

	orig_open = dlsym(RTLD_NEXT, "open");
	orig_close = dlsym(RTLD_NEXT, "close");
	orig_read = dlsym(RTLD_NEXT, "read");
	orig_write = dlsym(RTLD_NEXT, "write");
	orig_lseek = dlsym(RTLD_NEXT, "lseek");
	orig_stat = dlsym(RTLD_NEXT, "stat");
	orig_xstat = dlsym(RTLD_NEXT, "__xstat");
	orig_unlink = dlsym(RTLD_NEXT, "unlink");
	orig_getdirentries = dlsym(RTLD_NEXT, "getdirentries");
	orig_getdirtree = dlsym(RTLD_NEXT, "getdirtree");
	orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");
	// fprintf(stderr, "Init mylib\n");
	
	int rv;

	// Get environment variable indicating the ip/port of the server
	serverip = getenv("server15440");
	// printf("it is %s\n", serverip);
	serverport = getenv("serverport15440");
	if (serverport) port = (unsigned short)atoi(serverport);
	else port=18440;
	if (serverip == NULL) {
		serverip = "127.0.0.1";
	}

	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			// in case of error

	struct sockaddr_in srv;
	// setup address structure to indicate server port

	memset(&srv, 0, sizeof(srv));			// clear it first

	srv.sin_family = AF_INET;			// IP family

	srv.sin_addr.s_addr = inet_addr(serverip);	// server IP address

	srv.sin_port = htons(port);			// server port

	rv = connect(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	// Check the return value to make sure connection succeeds.
	if (rv < 0) {
		err(1, 0);
	}
}