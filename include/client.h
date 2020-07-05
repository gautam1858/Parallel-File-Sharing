#include <vector>
#ifndef CLIENT_H
#define CLIENT_H

#define initial_header_len 5
using std::vector;

class client_operations : public server_operations{	

private: 
	vector <host_info> connected_list;


	vector <std::string> send_cmd_buffer;
	int connected_list_idx;

public :
//client functions
	client_operations ();
	int connect_to_port (const char* host, const char * port);
	void recv_requests_client (int file_desc);
	void add_connection_list (int file_desc, const char* port);
	char* return_port_from_peer_list (int peer_fd);
	bool is_valid_peer (const char* host);
	void send_download_command (int file_desc, const char *send_cmd);
	void set_connection_on (int clientfd, bool val);
	void handle_rem_downloads (int clientfd);
	void add_st_time (int file_desc, double st_time);
	void remove_exclaimation_from_hostname (char *token);
	double st_time (int file_desc);
	bool is_connection_present (const char* host, const char* port);
	bool is_download_on (int clientfd);
	void split_return (const char* str, char c,  char* token);
	char* remove_from_connected_list (int file_desc, char *host);
	void send_file_over_socket (int clientfd, const char* filename);
	int return_first_occr (const char* str, char c);
	void terminate_client (int idx);
	void inline recv_stdin_client (int eventfd);
	void listen_to_requests (int sfd);
	void unpack_peers (char *token);
	void recv_and_write_file (int clientfd, unsigned char* rem_buf);
	void inline wait_for_event (int eventfd, struct epoll_event* event_array, int sfd);
};
#endif