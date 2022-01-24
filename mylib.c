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


// Sending and receiving message from server
void connect_message(char *buf) {
	char *serverport;
	unsigned short port;
	int sockfd, rv;
	char *serverip;

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
	rv = send(sockfd, buf, strlen(buf), 0);
	orig_close(sockfd);
}

// Sending and receiving message from server
void send_message(char *buf, int total_length) {
	char *serverport;
	unsigned short port;
	int sockfd, rv;
	char *serverip;

	// Get environment variable indicating the ip/port of the server
	serverip = getenv("server15440");
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

	// send out parameters packed in buf
	rv = send(sockfd, (buf), total_length, 0);

	// // receive from the server of return values
	// // message protocol: total_return_size + return message
	// char *size_pointer = malloc(sizeof(int));
	// while ( (rv=recv(sockfd, size_pointer, sizeof(int), 0)) > 0) {
	// 	if (rv < 0 || rv >= 4) {
	// 		break;
	// 	}
	// 	printf("received: %d bytes\n", rv);
	// }
	// int reply_size = *size_pointer;

	// printf("reply size is %d", reply_size);

	orig_close(sockfd);
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
	// assign opcode of "open" as 0
	int opcode = 66;
	int starter = 0;

	// overall size of message, protocol:
	// itself + opcode + pathname size + pathname + flag + mode_t
	int total_length = 4 * sizeof(int) + strlen(pathname) + sizeof(mode_t);
    char message[total_length];

	memcpy(message + starter, &total_length, sizeof(int));
	starter += sizeof(int);
	// set opcode
	memcpy(message + starter, &opcode, sizeof(int));
	starter += sizeof(int);
	// set flag
	memcpy(message + starter, &flags, sizeof(int));
	starter += sizeof(int);
	// set mode_t
	memcpy(message + starter, &m, sizeof(mode_t));
	starter += sizeof(mode_t);
    // send message to server, indicating type of operation

	// set header (size) of pathname
	int pathname_size = strlen(pathname);
	memcpy(message + starter, &pathname_size, sizeof(int));
	starter += sizeof(int);

	// set pathname
	memcpy(message + starter, pathname, pathname_size);
	starter += pathname_size;
	
	printf("open parameters are: %s, %d, %d\n", pathname, flags, m);
	send_message(message, total_length);
	return orig_open(pathname, flags, m);
}

int close(int fd) {
	char *message = "close\n";
	connect_message(message);
	return orig_close(fd);
}

ssize_t read(int fd, void *buf, size_t count) {
	char *message = "read\n";
	connect_message(message);
	return orig_read(fd, buf, count);
}

ssize_t write (int fd, const void *buf, size_t count) {
	char *message = "write\n";
	connect_message(message);
	return orig_write(fd, buf, count);
}

off_t lseek(int fd, off_t offset, int whence) {
	char *message = "lseek\n";
	connect_message(message);
	return orig_lseek(fd, offset, whence);
}

int stat (const char *pathname, struct stat *statbuf) {
	char *message = "stat\n";
	connect_message(message);
	return orig_stat(pathname, statbuf);
}

int __xstat(int ver, const char * path, struct stat * stat_buf) {
	char *message = "__xstat\n";
	connect_message(message);
	return orig_xstat(ver, path, stat_buf);
}

int unlink (const char *pathname) {
	char *message = "unlink\n";
	connect_message(message);
	return orig_unlink(pathname);
}
ssize_t getdirentries (int fd, char *buf, size_t nbytes, off_t *basep) {
	char *message = "getdirentries\n";
	connect_message(message);
	return orig_getdirentries(fd, buf, nbytes, basep);
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
}

