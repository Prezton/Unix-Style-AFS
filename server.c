#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>



#define MAXMSGLEN 100

int main(int argc, char**argv) {
	char *msg="Hello from server";
	char *serverport;
	unsigned short port;
	int sockfd, sessfd, rv, i;
	struct sockaddr_in srv, cli;
	socklen_t sa_size;
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) port = (unsigned short)atoi(serverport);
	else port=18440;
	
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			// in case of error
	
	// setup address structure to indicate server port
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;			// IP family
	srv.sin_addr.s_addr = htonl(INADDR_ANY);	// don't care IP address
	srv.sin_port = htons(port);			// server port

	// bind to our port
	rv = bind(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);
	
	// start listening for connections
	rv = listen(sockfd, 5);
	if (rv<0) err(1,0);
	sa_size = sizeof(struct sockaddr_in);

	// main server loop, handle clients one at a time, quit after 10 clients
	while (1) {
		// wait for next client, get session socket
		// printf("SERVER MAIN LOOP START\n");
		sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);
		if (sessfd<0) err(1,0);
		// char *buf;
		while (1) {

		char *size_pointer = malloc(sizeof(int));
		char *received_size = malloc(sizeof(int));
		int bytes_received = 0;

		while ( (rv=recv(sessfd, size_pointer, sizeof(int) - bytes_received, 0)) > 0) {
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
			close(sessfd);
			fprintf(stderr, "1st: receive message rv <= 0\n");
			break;
		}
		// recv(sessfd, size_pointer, sizeof(int), 0);
		int total_length = *((int *)received_size);
		if (total_length <= 0) {
			fprintf(stderr, "rv is: %d, total length is %d, less than 0: this should not happen!!\n",rv, total_length);
			close(sessfd);
			break;
		}
		// subtract total_length itself
		fprintf(stderr, "server: received %d bytes\n", total_length);
		total_length -= sizeof(int);

		char *buf = malloc(total_length);
		char *received_message = malloc(total_length);
		bytes_received = 0;
		while ( (rv=recv(sessfd, buf, total_length - bytes_received, 0)) > 0) {
			// create message from buf
			memcpy(received_message + bytes_received, buf, rv);
			bytes_received += rv;
			// enough bytes have been received
			if (bytes_received >= total_length) {
				break;
			}
		}
		// check validity
		if (rv<=0) {
			close(sessfd);
			fprintf(stderr, "2nd: receive message rv <=0\n");
			break;
		}
		int opcode = *((int *)received_message);
		// printf("opcode is %d\n", opcode);
		// either client closed connection, or error
		
		if (opcode == 66) {
			// "open"
			// message protocol:
			// opcode + flag + pathname size + mode_t + pathname
			fprintf(stderr, "open\n");
			int received_flag = *((int *)received_message + 1);
			// printf("flag is %d\n", received_flag);

			int pathname_size = *((int *)received_message + 2);
			// printf("pathname size is: %d\n", pathname_size);

			mode_t received_mode;
			memcpy(&received_mode, received_message + 3 * sizeof(int), sizeof(mode_t));

			// int received_mode = *((int *)received_message + 2);

			char *pathname = malloc(pathname_size);
			memcpy(pathname, (received_message + 12 + sizeof(mode_t)), pathname_size * sizeof(char));
			// strcpy(pathname, (received_message+4));
			// printf("pathname is: %s\n", pathname);
			fprintf(stderr, "server received open,flag is %d, mode is %d, pathname is %s\n", received_flag, received_mode, pathname);

			// make procedure call
			int fd = open(pathname, received_flag, received_mode);
			int err_num = errno;

			// send return value header back
			int return_size = 2 * sizeof(int);
			char *reply_message = malloc(3 * sizeof(int));
			memcpy(reply_message, &return_size, sizeof(int));
			memcpy(reply_message + sizeof(int), &fd, sizeof(int));
			memcpy(reply_message + 2 * sizeof(int), &err_num, sizeof(int));
			// send return value back
			fprintf(stderr, "server sent back open,fd is %d errno is %d\n", fd, err_num);

			send(sessfd, reply_message, 3 * sizeof(int), 0);
			
		} else if (opcode == 67) {
			fprintf(stderr, "close\n");
			int received_fd = *((int *)received_message + 1);
			// printf("close received fd is %d\n", received_fd);
			fprintf(stderr, "server received close,fd is %d\n", received_fd);

			int result = close(received_fd);
			int err_num = errno;

			// send return value header back
			int return_size = 2 * sizeof(int);
			// pack together
			char *reply_message = malloc(3 * sizeof(int));
			memcpy(reply_message, &return_size, sizeof(int));
			memcpy(reply_message + sizeof(int), &result, sizeof(int));
			memcpy(reply_message + 2 * sizeof(int), &err_num, sizeof(int));
			// send return value back
			fprintf(stderr, "server sent back close,result is %d, errno is %d\n", received_fd, err_num);

			send(sessfd, reply_message, 3 * sizeof(int), 0);

		} else if (opcode == 68) {
			// printf("read\n");
		} else if (opcode == 69) {
			// "write", opcode == 69
			// message protocol: 
			// opcode + fd + count + buf
			fprintf(stderr, "write\n");

			int received_fd = *((int *)received_message + 1);
			// printf("fd is %d\n", received_fd);
			// size_t received_count = *((size_t *)received_message + 2);

			// int buf_size = *((int *)received_message + 2);
			// printf("buf size is: %d\n", buf_size);

			size_t received_count;
			memcpy(&received_count, received_message + 2 * sizeof(int), sizeof(size_t));

			char *buf = malloc(received_count);
			memcpy(buf, ((char *)received_message + 8 + sizeof(size_t)), received_count * sizeof(char));

			// strcpy(pathname, (received_message+4));
			// strcpy(buf + buf_size, "\0");
			// printf("buf is :%s\n", buf);
			fprintf(stderr, "server received write,fd is %d, count is %lu, buf is %s\n", received_fd, received_count, buf);

			// make procedure call
			ssize_t num_write = write(received_fd, buf, received_count);
			int err_num = errno;

			// send return value header back
			// not an integer!!! return_size!!
			int return_size = sizeof(int) + sizeof(ssize_t);

			// send return value back
			char *reply_message = malloc(2 * sizeof(int) + sizeof(ssize_t));
			memcpy(reply_message, &return_size, sizeof(int));
			memcpy(reply_message + sizeof(int), &err_num, sizeof(int));
			memcpy(reply_message + 2 * sizeof(int), &num_write, sizeof(ssize_t));
			fprintf(stderr, "server sent back write,err_num is %d, bytes written is %lu\n", err_num, num_write);

			send(sessfd, reply_message, 2 * sizeof(int) + sizeof(ssize_t), 0);
		} 
		else {
			fprintf(stderr, "no corresponding opcode: %d\n", opcode);
			close(sessfd);
			break;
		}

		// close(sessfd);
		free(buf);
		free (size_pointer);
	}
	}
	// close socket
	close(sockfd);

	return 0;
}