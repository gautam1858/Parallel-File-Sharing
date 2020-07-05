#include "host_one.h"
#include <vector>
#ifndef SERVER_H
#define SERVER_H

//packet size for the file transfer
#define PACKET_SIZE 1024

//packet size for the non file message transfers.
#define MAXMSGSIZE 512

#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif

using std::vector;
void sigchld_handler(int s);

class server_operations{

//peer list at the server
//a class level member to avoid complications
protected: 
	vector <host_info> peer_list;
	int peer_idx;

//Server functions
public: 
	server_operations();
	void make_socket_non_blocking (int sfd);
	void peer_info(int peer_fd, const char* port);
	int sendall(int s,unsigned char *buf, int len);
	char* my_ip (char *ip_addr);
	void recv_stdin_client(int eventfd);
	void toUpper(char *str);
	void make_socket_blocking (int sfd);
	void make_entry_to_epoll(int sfd, int eventid);
	char* remove_from_peer_list(int file_desc,char *host);
	void recv_requests_server(int clientfd);
	void send_peer_list();
	void inline wait_for_event(int eventfd, struct epoll_event* event_array,int sfd);
	void listen_to_requests(int sfd);
	void *get_in_addr(struct sockaddr *sa);
	int server_setup(const char* port);
};

#endif
