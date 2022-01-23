#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

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
		// get messages and send replies to this client, until it goes away
		while ( (rv=recv(sessfd, size_pointer, sizeof(int), 0)) > 0) {
			if (rv < 0 || rv >= 4) {
				break;
			}
			printf("received: %d bytes\n", rv);
			// send reply
			// printf("server replying to client: %s\n", msg);
			// send(sessfd, msg, strlen(msg), 0);	// should check return value
		}
		int total_length = *size_pointer;
		printf("server: total size is: %d\n", total_length);

		char *buf = malloc(total_length);
		char *mark = buf;
		while ( (rv=recv(sessfd, buf, total_length - 4, 0)) > 0) {
			// send reply
			// printf("server replying to client: %s\n", msg);
			// send(sessfd, msg, strlen(msg), 0);	// should check return value
			printf("%s", buf);
			buf += rv;

		}
		printf("server: message is%s\n", mark);

		char *opcode_pointer = malloc(sizeof(int));
		int opcode = *opcode_pointer;
		// either client closed connection, or error
		if (rv<0) err(1,0);
		close(sessfd);

		free (size_pointer);
		free(opcode_pointer);
	}
	
	// close socket
	close(sockfd);

	return 0;
}

