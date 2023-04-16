#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <sys/queue.h>
	
#define MAXBUFFER 1024
#define MAXWORD 20
#define MAXTIME 20
#define SELECT_TIMEOUT (100 * 1000)

// An entry in the send queue.
struct sq_entry {
	TAILQ_ENTRY(sq_entry) entries;		// Linked list pointer.
	char rp[129+MAXWORD+MAXBUFFER+MAXTIME]; // The actual packet data along with the header.
	unsigned int seq_num;			// Sequence number of the outgoing packet.
	time_t tsec;				// The time when the packet was added to the send queue.
	int sockfd;				// Socket descriptor on which the packet was sent.
	int num_retrans;			// The number of times the packet has been retransmitted.
};

// An entry in the receive queue.
struct rq_entry {
	TAILQ_ENTRY(rq_entry) entries;		// Linked list pointer
	char rp[129+MAXWORD+MAXBUFFER+MAXTIME]; // The actual packet data along with the header.
	unsigned int seq_num;			// Sequence number of the incoming packet.
};

void chat(int i, int sd,char *user_name);		
void connect_server(int *sd, struct sockaddr_in *server_addr, char * server, int port);
void broadcast_message(int j, int i, int sd, int nbytes_recvd, char *recv_buf, fd_set *master);
void message_send(int i, fd_set *server, int sd, int max_sd);
void accept_connection(fd_set *server, int *fdmax, int sd, struct sockaddr_in *client_addr);
void create_listener(int *sd, struct sockaddr_in *my_addr);
void add_user_time(char *Buffer,int user);
void check_retrans_timeout();
struct sq_entry *add_to_send_queue(int sockfd, char *buffer, int pl_size);
