#include "client_header.h"

unsigned long next_seq_num;
unsigned long expected_seq_num;

//Initalize the send queue and the receive queue.
TAILQ_HEAD(sq_head, sq_entry) shead = TAILQ_HEAD_INITIALIZER(shead);
TAILQ_HEAD(rq_head, rq_entry) rhead = TAILQ_HEAD_INITIALIZER(rhead);
struct sq_head *sheadp;
struct rq_head *rheadp;

void connect_server(int *sockfd, struct sockaddr_in *server_addr, char *server, int port)
{
	if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "%s:in %s socket error %d",__FILE__,__func__,__LINE__);
		perror("Socket");
		exit(1);
	}
	server_addr->sin_family = AF_INET;
	//Port number that we received from the user.
	server_addr->sin_port = htons(port);
	//IP address that we received from the user.
	server_addr->sin_addr.s_addr = inet_addr(server);
	memset(server_addr->sin_zero, '\0', sizeof server_addr->sin_zero);

	if(connect(*sockfd, (struct sockaddr *)server_addr, sizeof(struct sockaddr)) == -1) {
		fprintf(stderr, "%s:in %s connect error %d",__FILE__,__func__,__LINE__);
		perror("connect");
		exit(1);
	}
	//Initalize the send queue and the receive queue.
	TAILQ_INIT(&shead);
	TAILQ_INIT(&rhead);
}	

void check_retrans_timeout()
{
	struct sq_entry *sn1, *sn2;

	sn1 = TAILQ_FIRST(&shead);
	//Wait for 5 seconds before sending a retransmission.
	while (sn1 != NULL) {
		sn2 = TAILQ_NEXT(sn1, entries);
		if (time(NULL) > (sn1->tsec + 5)) {
			if (sn1->num_retrans >= 25) {
				//Connection timed out. Close the connection
				fprintf(stderr, "Closing connection due to too many timeouts.\n");
				close(sn1->sockfd);
			} else {
				//send_retransmission
				fprintf(stderr, "Sending retransmission for seq_num. %d\n", sn1->seq_num);
				send(sn1->sockfd, sn1->rp, strlen(sn1->rp), MSG_DONTWAIT);
				sn1->num_retrans++;
				sn1->tsec = time(NULL);
				usleep(100 * 1000);
			}
		}
		sn1 = sn2;
	}
}

struct sq_entry *add_to_send_queue(int sockfd, char *payload, int pl_size)
{

	struct sq_entry *entry;
	int num_char;

	//Add the packet to send queue. This will be useful in sending retranmission.
	entry = malloc(sizeof(struct sq_entry));
	if (!entry) {
		return NULL;
	}

	memset(entry, 0, sizeof(struct sq_entry));
	//Add sequence number and ack to the packet. Since we have data in this packet, pure ack is set to 0.
	num_char = sprintf(entry->rp, "%lu,%lu,%d:", next_seq_num, expected_seq_num, 0);
	memcpy(entry->rp + num_char, payload, pl_size);
	entry->seq_num = next_seq_num;
	entry->tsec = time(NULL);
	entry->sockfd = sockfd;
	
	TAILQ_INSERT_TAIL(&shead, entry, entries);
	
	//Increase the sequence number for the next packet.
	next_seq_num++;

	return entry;
}

int process_recv_packet(int sockfd, char *packet, int pkt_size)
{
	char *seq_num;
	char *ack_num;
	char *header;
	char *data;
	char *pure_ack;
	struct rq_entry *entry;
	char rp[129+MAXWORD+MAXBUFFER+MAXTIME];
	struct sq_entry *sn1, *sn2;
	struct rq_entry *rn1, *rn2;

	// Split the header from the actual packet data.
	header = strtok(packet, ":");
	if (header == NULL) {
		return 0;
	}
	data = packet + strlen(header) + 1;

	// Extract the sequence number from the header.
	seq_num = strtok(header, ",");
	if (seq_num == NULL) {
		// Probably the header is corrupted, drop the packet.
		return 0;
	}

	// Extract the ack number from the header.
	ack_num = strtok(NULL, ",");

	if (ack_num == NULL) {
		// Probably the header is corrupted, drop the packet.
		return 0;
	}

	// Check if it is a pure ack. Packets with pure ack don't contain any data.
	pure_ack = ack_num + strlen(ack_num) + 1;

	//Check if we received a packet out of order. If the sequence number on the packet is greater than the expected sequenece number then we have received it out of order.
	if (atoi(seq_num) > expected_seq_num) {
		//If the packet is out of order then add it to the receiver buffer queue. The receive buffer queue must be sorted by the sequence number
		entry = malloc(sizeof(struct rq_entry));
		if (!entry) {
			return 1;
		}
		memset(entry, 0, sizeof(struct rq_entry));
		// Copy the packet data only.
		memcpy(entry->rp, data, pkt_size - strlen(header) - 1);

		rn1 = TAILQ_FIRST(&rhead);
		rn2 = TAILQ_LAST(&rhead, rq_head);
		if (rn2 && atoi(seq_num) > rn2->seq_num) {
			// Add to the end of the list as the sequence number on the packet is greater than sequence number of the last packet in the receive queue..
			entry->seq_num = atoi(seq_num);
			TAILQ_INSERT_AFTER(&rhead, rn2, entry, entries);
			fprintf(stderr,"Adding seqnum %d after %d\n", atoi(seq_num), rn2->seq_num);
		} else if (rn1) {
			//Add the packet before the packet with sequence number greater than this packet's sequence number.
			while (rn1 != NULL) {
				rn2 = TAILQ_NEXT(rn1, entries);
				if (rn1->seq_num == atoi(seq_num)) {
					//If this sequeunce number is already present then we have possibly received a duplicate packet. Just ignore it.
					fprintf(stderr, "Sequence number %d already present. Possible duplicate.\n", rn1->seq_num);	
					break;
				} else if (rn1->seq_num > atoi(seq_num)) {
					//Add the packet before the packet with sequence number greater than this packet's sequence number.
					fprintf(stderr,"Adding seqnum %d before %d\n", atoi(seq_num), rn1->seq_num);
					entry->seq_num = atoi(seq_num);
					TAILQ_INSERT_BEFORE(rn1, entry, entries);
					break;
				}
				rn1 = rn2;
			}
		} else {
			//We reached here because there is no element in the receive queue.
			fprintf(stderr,"Adding seqnum %d at start\n", atoi(seq_num));
			entry->seq_num = atoi(seq_num);
			TAILQ_INSERT_HEAD(&rhead, entry, entries);
		}

		fprintf(stderr,"Expected seq num was: %lu\n", expected_seq_num);
	} else  {
		//Recieved a packet in correct order.
		//If this is not a pure ack then we need to send the data to the user.
		if (atoi(pure_ack) == 0) {
			if (atoi(seq_num) == expected_seq_num) {
				printf("%s" , data);
				fflush(stdout);
				//Increase the next expected sequence number.
				expected_seq_num++;
			}

			fprintf(stderr, "Seq num %d processed.\n", atoi(seq_num));
			fprintf(stderr, "Ack num %d processed.\n", atoi(ack_num));
			//Send pure ack, don't increase sequence number.
			memset(rp, 0, sizeof(rp));
			sprintf(rp, "%lu,%d,%d:\n%c", next_seq_num, atoi(seq_num)+1, 1,'\0');
			fprintf(stderr, "Sending pure ack %s\n",rp);
			//Send a pure ack for this packet 
			send(sockfd, rp, strlen(rp), MSG_DONTWAIT);

			rn1 = TAILQ_FIRST(&rhead);
			// Check if we need to process any packets that are already present in the receiver buffer queue.
			while (rn1 != NULL) {
				rn2 = TAILQ_NEXT(rn1, entries);
				if (rn1->seq_num == expected_seq_num) {
					printf("%s",rn1->rp);
					fflush(stdout);
					fprintf(stderr, "Seq num %d processed.\n", rn1->seq_num);
					TAILQ_REMOVE(&rhead, rn1, entries);
					//Send ack for buffered packet.
					memset(rp, 0, sizeof(rp));
					sprintf(rp, "%lu,%d,%d:\n%c", next_seq_num, rn1->seq_num+1, 1, '\0');
					fprintf(stderr, "Sending Pure ack %s\n",rp);
					//Send a pure ack for this packet and remove it from the receive buffer queue.
					send(sockfd, rp, strlen(rp), MSG_DONTWAIT);
					free(rn1);
					expected_seq_num++;
				}
				rn1 = rn2;
			}

			fprintf(stderr, "Next expected seq num: %lu\n", expected_seq_num);
		} else {
			fprintf(stderr, "Pure ack is received: %d\n", atoi(pure_ack));
			//Remove this packet from the send queue since ack have been received.
			fprintf(stderr, "data %s\n", data);

			sn1 = TAILQ_FIRST(&shead);
			//Remove all the packets which have sequence number lower than the ack.
			while (sn1 != NULL) {
				sn2 = TAILQ_NEXT(sn1, entries);
				//if (sn1->seq_num <= atoi(ack_num)) {
				if (sn1->seq_num < atoi(ack_num)) {
					//fprintf(stderr, "Ack num %d removed.\n", atoi(ack_num));
					fprintf(stderr, "Ack num %d removed.\n", sn1->seq_num);
					TAILQ_REMOVE(&shead, sn1, entries);
					free(sn1);
				}
				sn1 = sn2;
			}
		}
	}
	return 0;
}

void chat(int count, int sockfd,char *user_name)
{
	//char send_buf[MAXBUFFER];
	char recv_buf[MAXBUFFER];
	char buffer[MAXWORD+MAXBUFFER+MAXTIME];
	//char *user = user_name;
	int num_byte_recvd;
	struct sq_entry *sentry;

	if (count == 0) {
		memset(buffer, 0, sizeof(buffer));
		// Get message typed by the user.
		fgets(buffer, MAXBUFFER, stdin);
		if (strcmp(buffer, "quit\n") == 0) {
			fprintf(stderr, "Exiting program.\n");
			exit(0);
		}

		if (strlen(buffer) == 0) {
			return;
		}

		// Add this packet to the send buffer queue before sending it on network.
		sentry = add_to_send_queue(sockfd, buffer, strlen(buffer));
		// Send the packet on network.
		send(sockfd, (char *)(&(sentry->rp)), strlen(sentry->rp), MSG_DONTWAIT);
		usleep(100 * 1000);
	} else {
		//Receive the data.
		num_byte_recvd = recv(sockfd, recv_buf, MAXBUFFER, 0);
		recv_buf[num_byte_recvd] = '\0';
		usleep(100 * 1000);
		// Analyze and Process the data received.
		process_recv_packet(sockfd, recv_buf, num_byte_recvd);
	}
}

int main(int argc, char *argv[])
{
	int sd, max_sd, count;
	struct sockaddr_in server_addr;
	fd_set server,read_sd;
	char user_name[MAXWORD];
	struct timeval tv;
	int sel_ret = 0;

	if (argc < 3) {
		fprintf(stderr, "Usage: ./client <server_ip> <port_no>\n");
		exit(-1);
	}
	// Connect to the server.
	connect_server(&sd, &server_addr, argv[1], atoi(argv[2]));
	fprintf(stderr, "Connected to server.\n");
	fflush(stdin);
	//Initialize the descriptors for select.
	FD_ZERO(&server);
	FD_ZERO(&read_sd);
	FD_SET(0, &server);
	FD_SET(sd, &server);
	max_sd = sd;


	while (1) {
		read_sd = server;
		// Timeout value for select system call. Select will wait for this much time and if no data is received on any descriptor select will return with value 0.
		tv.tv_sec = 0;
		tv.tv_usec = SELECT_TIMEOUT;
		/* Wait for data on any socket descriptors or standard input.*/
		sel_ret = select(max_sd+1, &read_sd, NULL, NULL, &tv);
		if (sel_ret == -1) {
			fprintf(stderr,"%s select error %d",__FILE__,__LINE__);
			perror("select");
			exit(-1);
		}
		
		/* Check if we need to retransmit some packets */
		check_retrans_timeout();	
		if (sel_ret == 0) {
			continue;
		}

		for (count = 0; count <= max_sd; count++ ) {
			//Process only if something happened on this descriptor. read_sd was set by select system call.
			if(FD_ISSET(count, &read_sd)) {
				chat(count, sd, user_name);
			}
		}
	}
	fprintf(stderr, "client got disconnected\n");
	close(sd);
	return 0;
}
