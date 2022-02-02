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
#include <sys/stat.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>
#include <../include/dirtree.h>

#define MAXMSGLEN 100
char *tree_string;
int tree_offset;

void serialize_tree(struct dirtreenode *root);
int get_tree_size(struct dirtreenode *root);

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
			// "read", opcode == 68
			fprintf(stderr, "read\n");
			int received_fd = *((int *)received_message + 1);
			size_t received_count;
			memcpy(&received_count, received_message + 2 * sizeof(int), sizeof(size_t));
			fprintf(stderr, "server received read,fd is %d, count is %lu\n", received_fd, received_count);

			char tmp[received_count];
			ssize_t num_read = read(received_fd, tmp, received_count);
			int err_num = errno;
			// return size = errno + num_read + strlen(tmp)
			int return_size = sizeof(int) + sizeof(ssize_t) + received_count;

			char *reply_message = malloc(sizeof(int) + return_size);
			memcpy(reply_message, &return_size, sizeof(int));
			memcpy(reply_message + sizeof(int), &err_num, sizeof(int));
			memcpy(reply_message + 2 * sizeof(int), &num_read, sizeof(ssize_t));
			memcpy(reply_message + 2 * sizeof(int) + sizeof(ssize_t), tmp, received_count);
			send(sessfd, reply_message, sizeof(int) + sizeof(int) + sizeof(ssize_t) + received_count, 0);
			// strcpy(tmp + received_count, "\0");

			fprintf(stderr, "server sent back read, err_num is %d, bytes read is %lu, content is:%s\n", err_num, num_read, tmp);

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
		} else if (opcode == 70) {
			// "lseek", opcode == 70
			// message protocol: 
			// opcode + fd + whence + offset
			fprintf(stderr, "lseek\n");
			int received_fd = *((int *)received_message + 1);
			int received_whence = *((int *)received_message + 2);
			off_t received_offset;
			memcpy(&received_offset, (received_message + 3 * sizeof(int)), sizeof(off_t));
			fprintf(stderr, "server received lseek, fd is %d, whence is %d, offset is %ld\n", received_fd, received_whence, received_offset);
			off_t result = lseek(received_fd, received_offset, received_whence);
			int err_num = errno;
			
			// return size = errno (int) + result (off_t)
			int return_size = sizeof(off_t) + sizeof(int);
			// message-to-be-replied size include return_size itself
			char *reply_message = malloc(2 * sizeof(int) + sizeof(off_t));
			memcpy(reply_message, &return_size, sizeof(int));
			memcpy(reply_message + sizeof(int), &err_num, sizeof(int));
			memcpy(reply_message + 2 * sizeof(int), &result, sizeof(off_t));
			fprintf(stderr, "server sent back lseek, err_num is %d, result is %ld\n", err_num, result);
			send(sessfd, reply_message, 2 * sizeof(int) + sizeof(off_t), 0);
		} else if (opcode == 71) {
			// "__xstat", opcode == 71
			// message protocol: 
			// opcode + ver + path_size + path
			fprintf(stderr, "__xstat\n");
			int received_ver = *((int *)received_message + 1);
			int received_pathsize = *((int *)received_message + 2);
			char *received_path = malloc(received_pathsize);
			memcpy(received_path, received_message + 3 * sizeof(int), received_pathsize);
			fprintf(stderr, "server received __xstat, ver is %d, pathsize is %d, path is %s\n", received_ver, received_pathsize, received_path);
			
			struct stat *tmp_stat_buf = malloc(sizeof(struct stat));
			int result = __xstat(received_ver, received_path, tmp_stat_buf);
			int err_num = errno;

			// return size + errno + result + stat_buf
			int return_size = 2 * sizeof(int) + sizeof(struct stat);
			char *reply_message = malloc(3 * sizeof(int) + sizeof(struct stat));
			memcpy(reply_message, &return_size, sizeof(int));
			memcpy(reply_message + sizeof(int), &err_num, sizeof(int));
			memcpy(reply_message + 2 * sizeof(int), &result, sizeof(int));
			memcpy(reply_message + 3 * sizeof(int), tmp_stat_buf, sizeof(struct stat));
			fprintf(stderr, "server sent back __xstat, err_num is %d, result is %d\n", err_num, result);
			send(sessfd, reply_message, 3 * sizeof(int) + sizeof(struct stat), 0);

		} else if (opcode == 72) {
			fprintf(stderr, "unlink\n");

			// opcode + path size + pathname
			int received_pathsize = *((int *)received_message + 1);
			char *received_path = malloc(received_pathsize);
			memcpy(received_path, received_message + 2 * sizeof(int), received_pathsize);
			fprintf(stderr, "server received unlink, path size is %d, path is %s\n", received_pathsize, received_path);
			int result = unlink(received_path);
			int err_num = errno;
			
			char *reply_message = malloc(3 * sizeof(int));
			int return_size = 2 * sizeof(int);
			memcpy(reply_message, &return_size, sizeof(int));
			memcpy(reply_message + sizeof(int), &err_num, sizeof(int));
			memcpy(reply_message + 2 * sizeof(int), &result, sizeof(int));
			fprintf(stderr, "server sent back unlink, err_num is %d, result is %d\n", err_num, result);
			send(sessfd, reply_message, 3 * sizeof(int), 0);

		} else if (opcode == 73) {
			fprintf(stderr, "getdirentries\n");

			// opcode + fd + nbytes + basep
			int received_fd = *((int *)received_message + 1);
			size_t received_nbytes;
			memcpy(&received_nbytes, received_message + 2 * sizeof(int), sizeof(size_t));
			off_t received_basep;
			memcpy(&received_basep, received_message + 2 * sizeof(int) + sizeof(size_t), sizeof(off_t));

			fprintf(stderr, "server received getdirentries, fd is %d, nbytes is %ld, basep is %ld\n", received_fd, received_nbytes, received_basep);
			char *tmp_buf = malloc(received_nbytes);
			ssize_t bytes_read = getdirentries(received_fd, tmp_buf, received_nbytes, &received_basep);
			int err_num = errno;
			int bytes_read_length = bytes_read;
			// fprintf(stderr, "test1 %d", bytes_read);
			if (bytes_read == -1) {
				bytes_read_length = 0;
			}
			// reply_message = errno + bytes_read + tmp_buf
			int return_size = sizeof(int) + sizeof(ssize_t) + bytes_read_length;
			char *reply_message = malloc(2 * sizeof(int) + sizeof(ssize_t) + bytes_read_length);
			memcpy(reply_message, &return_size, sizeof(int));
			memcpy(reply_message + sizeof(int), &err_num, sizeof(int));
			memcpy(reply_message + 2 * sizeof(int), &bytes_read, sizeof(ssize_t));
			memcpy(reply_message + 2 * sizeof(int) + sizeof(ssize_t), tmp_buf, bytes_read_length);
			fprintf(stderr, "server sent back getdirentries, err_num is %d, bytes_read is %ld, tmp_buf is %s\n", err_num, bytes_read, tmp_buf);

			send(sessfd, reply_message, 2 * sizeof(int) + sizeof(ssize_t) + bytes_read_length, 0);


		} else if (opcode == 74) {
			fprintf(stderr, "getdirtree\n");
			// opcode + path size + path
			int received_pathsize = *((int *)received_message + 1);
			char *received_path = malloc(received_pathsize);
			memcpy(received_path, received_message + 2 * sizeof(int), received_pathsize);
			fprintf(stderr, "server received getdirtree, path size is %d, path is %s\n", received_pathsize, received_path);
			struct dirtreenode* root = getdirtree(received_path);
			int err_num = errno;

			// traverse to get tree size
			int tree_size = get_tree_size(root);
			fprintf(stderr, "tree string size is %d\n", tree_size);

			tree_string = malloc(tree_size);
			tree_offset = 0;
			serialize_tree(root);
			fprintf(stderr, "server sent out tree string, tree_string is: %s\n", tree_string);
			send(sessfd, &tree_size, sizeof(int), 0);
			send(sessfd, tree_string, tree_size, 0);
			free(tree_string);
			freedirtree(root);
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


int get_tree_size(struct dirtreenode *root) {
	int cur_size = 0;
	int name_size = strlen(root->name);
	char *name = root->name;
	int num_children = root->num_subdirs;

	// each node has 3 fields: name_size (int) + name (strlen(name)) + num_children (int)
	cur_size += (name_size + 2 * sizeof(int));
	int i = 0;
	for (i = 0; i < num_children; i++) {
		cur_size += get_tree_size(root->subdirs[i]);
	}
	return cur_size;
}


void serialize_tree(struct dirtreenode *root) {
	int name_size = strlen(root->name);
	char *name = root->name;
	int num_children = root->num_subdirs;

	memcpy(tree_string + tree_offset, &name_size, sizeof(int));
	fprintf(stderr, "name size is %d\n", *(tree_string + tree_offset));

	tree_offset += sizeof(int);
	memcpy(tree_string + tree_offset, &num_children, sizeof(int));
	fprintf(stderr, "children num is %d\n", *(tree_string + tree_offset));

	tree_offset += sizeof(int);
	memcpy(tree_string + tree_offset, name, name_size);
	fprintf(stderr, "name is %s\n", name);

	tree_offset += name_size;
	int i = 0;
	// fprintf(stderr, "name is %s\n", *(tree_string));
	for (i = 0; i < num_children; i++) {
		serialize_tree(root->subdirs[i]);
	}
}