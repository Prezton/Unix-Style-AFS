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
		sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);
		if (sessfd<0) err(1,0);
		// char *buf;
		char *size_pointer = malloc(sizeof(int));
		int bytes_received = 0;
		// get messages and send replies to this client, until it goes away
		while ( (rv=recv(sessfd, size_pointer, sizeof(int), 0)) > 0) {
			if (rv < 0 || rv >= 4) {
				break;
			}
			printf("received: %d bytes\n", rv);
		}
		int total_length = *size_pointer;
		printf("server: total size is: %d\n", total_length);

		char *buf = malloc(total_length);
		char *received_message = malloc(total_length);
		bytes_received = 0;
		while ( (rv=recv(sessfd, buf, total_length, 0)) > 0) {
			// check validity
			if (rv<0) {
				err(1,0);
				fprintf(stderr, "receive message error\n");
			};
			// create message from buf
			memcpy(received_message + bytes_received, buf, rv);
			bytes_received += rv;
			// enough bytes have been received
			if (bytes_received >= total_length) {
				break;
			}
		}

		char *opcode_pointer = malloc(sizeof(int));
		int opcode = *((int *)received_message);
		printf("opcode is %d\n", opcode);
		// either client closed connection, or error
		
		if (opcode == 66) {
			// "open"
			// message protocol:
			// opcode + flag + mode_t + pathname size + pathname

			int received_flag = *((int *)received_message + 1);
			printf("flag is %d\n", received_flag);
			int received_mode = *((int *)received_message + 2);
			printf("mode is %d\n", received_mode);

			int pathname_size = *((int *)received_message + 3);
			printf("pathname size is: %d\n", pathname_size);
			char *pathname = malloc(pathname_size + 1);
			memcpy(pathname, (received_message + 12 + sizeof(mode_t)), pathname_size * sizeof(char));
			// strcpy(pathname, (received_message+4));
			strcpy(pathname + pathname_size, "\0");
			printf("pathname is :%s\n", pathname);
			
			// make procedure call
			int fd = open(pathname, received_flag, received_mode);
			int err_num = errno;

			// send return value header back
			char *reply_size = malloc(sizeof(int));
			int return_size = 2 * sizeof(int);
			memcpy(reply_size, &return_size, sizeof(int));
			send(sessfd, reply_size, sizeof(int), 0);

			// send return value back
			char *reply_message = malloc(2 * sizeof(int));
			memcpy(reply_message, &fd, sizeof(int));
			memcpy(reply_message + sizeof(int), &err_num, sizeof(int));
			send(sessfd, reply_message, 2 * sizeof(int), 0);
		}

		close(sessfd);
		free(buf);
		free (size_pointer);
		free(opcode_pointer);
	}
	
	// close socket
	close(sockfd);

	return 0;
}

