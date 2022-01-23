// messages_info.h

// Messaging between client and server

struct client_info {
	int opcode;
    int msg_size;
    char *msg;
};

struct server_info {
    int errno;
    int msg_size;
    char *msg;
}
