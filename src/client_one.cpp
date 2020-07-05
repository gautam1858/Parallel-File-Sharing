/* client_one.cpp file
client Functions
 */

#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <climits>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <ctime>
#include <sys/epoll.h>
#include <fstream>
#include <errno.h>
#include "../include/server.h"
#include "../include/client.h"
#include <iomanip>
#include <cstring>

#define BACKLOG 10     // how many pending connections queue will hold

#define maxPeers 10	   //Maximum number of peers allowed to be connected to server

extern const char* server_port;		//Listening port of the server

#define default_server "timberlake.cse.buffalo.edu"

//Assignmnet operator for class host_info
host_info& host_info:: operator= (const host_info b){
	file_descriptor=b.file_descriptor;
	strcpy(hostname,b.hostname);
	strcpy(ipstr,b.ipstr);
	strcpy(port,b.port);
	return *this;

}

client_operations::client_operations(){
	peer_idx=0;
	peer_list.resize(10);
	connected_list.resize(10);
	connected_list_idx=0;
}

//Extracts and fills info about connected peer in connection_list
void client_operations::add_connection_list(int file_desc, const char* port){

	//Sock Addr
	struct sockaddr_storage addr;
	socklen_t len = sizeof addr;
	//Hostname
	char hostname[MAXMSGSIZE];
	//Ip address
	char ipstr[INET6_ADDRSTRLEN];
	//Service name
	char service[20];

	//Fill addr with info
	//Used examples from beej.us guide on getpeername, getnameinfo functions
	getpeername(file_desc, (struct sockaddr*)&addr, &len);

	if (addr.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;

		inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
	} else { // AF_INET6
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;

		inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
	}

	//Fill out the vector conncted_list
	getnameinfo((struct sockaddr*)&addr,sizeof addr,hostname,sizeof hostname, service, sizeof service,0);

	std::cout<<"connected to "<<hostname<<std::endl;

	connected_list.at(connected_list_idx).hostname=new char[strlen(hostname)];
	strcpy(connected_list.at(connected_list_idx).hostname,hostname);

	connected_list.at(connected_list_idx).port=new char[strlen(port)];
	strcpy(connected_list.at(connected_list_idx).port,port);

	strcpy(connected_list.at(connected_list_idx).ipstr,ipstr);

	connected_list.at(connected_list_idx).file_descriptor=file_desc;

	connected_list.at(connected_list_idx).connection_on=false;

	connected_list.at(connected_list_idx).download_st_time=0.0;

	connected_list_idx++;
}

//Sends download command to the given file_desc peer
void client_operations::send_download_command(int file_desc,const char *send_cmd){

	//set connection on with this peer to true
	set_connection_on(file_desc,true);

	//send the command to the peer
	int count=sendall(file_desc,(unsigned char *)send_cmd,MAXMSGSIZE);
	if(count!=0){
		perror("send");

	}
}

//Remove the peer from connection list and fill out it's host name
char* client_operations::remove_from_connected_list(int file_desc,char *host){

	for(int i=0;i<connected_list_idx;i++){
		if(connected_list.at(i).file_descriptor==file_desc){
			strcpy(host,connected_list.at(i).hostname);

			connected_list.at(i)=connected_list.at(--connected_list_idx);
		}
	}
	return (char *)host;
}

//Set connection on to value val for the peer
void client_operations::set_connection_on(int clientfd ,bool val){
	for(int i=0;i<connected_list_idx;i++){
		if(connected_list.at(i).file_descriptor==clientfd){
			connected_list.at(i).connection_on=val;
		}
	}

}

//check if download is on with this peer
bool client_operations::is_download_on(int clientfd){
	for(int i=0;i<connected_list_idx;i++){
		if(connected_list.at(i).file_descriptor==clientfd){
			return connected_list.at(i).connection_on;
		}
	}
	return false;
}

//check if connection exists with the peer to avoid duplicate connections
bool client_operations::is_connection_present(const char* host, const char* port){

	for(int i=0;i<connected_list_idx;i++){
		if(!strcmp(connected_list.at(i).hostname,host) && !strcmp(connected_list.at(i).port,port) ){
			return true;
		}
	}
	return false;
}

//Is given peer a valid peer to connect to checks against peer list received form the server
bool client_operations::is_valid_peer(const char* host){
	char temp_host[46]="::ffff:";
	strcat(temp_host,host);

	for (int i=0;i<peer_idx;i++){

		//for some reason I keep getting some garbage along with hostname for highgate.cse.buffalo.edu e.g. highgate.cse.buffalo.edu1 or !highgate.cse.buffalo.edu
		//so added this strstr check to see if hostname in peer_list is contained within hostname.
		char *str=strstr(peer_list.at(i).hostname,host);
		if((!strcmp(peer_list.at(i).hostname,host)) || (!strcmp(peer_list.at(i).ipstr,host)) || (!strcmp(peer_list.at(i).ipstr,temp_host) || str)){
			return true;
		}
	}
	return false;
}

//connect to host specified by host at port port
//copied from http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
int client_operations::connect_to_port(const char* host,const char * port)
{
	int sockfd, numbytes;
	char hostname[MAXMSGSIZE];
	char service[20];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int error;
	char s[INET6_ADDRSTRLEN];


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	//Avoid duplicate connection
	if(is_connection_present(host,port)){
		fprintf(stderr,"Connection is already present between peers");
		return -1;
	}

	if ((rv = getaddrinfo(host,port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 2;
	}

	// loop through all the results and connect to the first we can
	//Usual connect call
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		error=connect(sockfd, p->ai_addr, p->ai_addrlen);
		if(error==-1){
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);

	printf("client: connecting to %s\n", s);

	//add socket to connected_list
	add_connection_list(sockfd,port);
	freeaddrinfo(servinfo); // all done with this structure
	return sockfd;
}


//client's version of list to requets function
//Client overrides the server method due to small peculiarities of the client
//again socket part is copied from beej.us
void client_operations::listen_to_requests(int sfd){

	//Epoll eventfd
	int eventfd;

	//epoll event handler to add events to
	struct epoll_event event;
	struct epoll_event* event_array =new epoll_event[10] ;
	struct sockaddr_storage in_addr;
	struct sigaction sa;

	//listen
	if (listen(sfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	//create epoll eventfd
	eventfd = epoll_create (maxPeers);
	if (eventfd == -1)
	{
		perror ("epoll_create");
		abort ();
	}

	//Listen to stdin events
	make_entry_to_epoll(0,eventfd);

	//listen to own socket
	make_entry_to_epoll(sfd,eventfd);

	printf("client: waiting for connections...\n");
	wait_for_event(eventfd,event_array,sfd);

	delete event_array;

	close (sfd);
}


//Sends any download commands remaining for the given peer
void client_operations::handle_rem_downloads(int clientfd){
	set_connection_on(clientfd,false);
	while(!is_download_on(clientfd) && !(send_cmd_buffer.empty())){

		send_download_command(clientfd,send_cmd_buffer.front().c_str());
		send_cmd_buffer.erase(send_cmd_buffer.begin());

	}
}

//Adds start time of download to connected list.
void  client_operations::add_st_time(int file_desc,double st_time){

	for(int i=0;i<connected_list_idx;i++){
		if(connected_list.at(i).file_descriptor==file_desc){

			connected_list.at(i).download_st_time=st_time;

		}
	}
}

//returns start time from the connected list
double  client_operations::st_time(int file_desc){

	for(int i=0;i<connected_list_idx;i++){
		if(connected_list.at(i).file_descriptor==file_desc){

			return connected_list.at(i).download_st_time;
		}
	}
	return 0.0;
}

//returns index of first occurrence of a char c in the str
int client_operations::return_first_occr(const char* str, char c){

	for (int i=0;i<PACKET_SIZE;i++){
		if(str[i]==c){
			return i;
		}
	}
	return -1;
}

//puts the string into token until char c is encountered
void client_operations:: split_return(const char* str, char c, char *token){

	int i=0;
	int j=0;

	while((str[i]!=c)){
		*(token+j)=str[i++];
		j++;
	}
	token[j]='\0';
}

//send the file over socket
void client_operations::send_file_over_socket(int clientfd,const char* filename){

	//cont of the bytes sent etc. from return values of fread, send etc.
	int count;
	//data buffer
	unsigned char data_buffer[PACKET_SIZE];
	//count the total bytes sent
	int total=0;
	//count of header length
	int header_count=0;
	//file_size
	size_t file_size;
	struct stat filestatus;							//http://www.cplusplus.com/forum/unices/3386/
	stat( filename, &filestatus );
	file_size=filestatus.st_size;
	char file_head[initial_header_len]="File";
	//find time spent in upload
	struct timespec tstart={0,0}, tend={0,0};		//http://stackoverflow.com/questions/16275444/c-how-to-print-time-difference-in-accuracy-of-milliseconds-and-nanoseconds
	clock_gettime(CLOCK_MONOTONIC, &tstart);


	//File pointer open in binary mode.
	FILE* File=fopen(filename,"rb");

	if (!File) {
		//If file is not present then let the other side know by sending file size=-1
		perror ("Error opening file");

		//header to be sent to the receiver
		sprintf((char *)data_buffer,"File %s %d \r",filename,-1);
		data_buffer[PACKET_SIZE-1]='\0';

		count=sendall(clientfd,data_buffer,sizeof(data_buffer));

		if(count!=0){
			perror ("send");
			close(clientfd);
		}
		return;
	}

	fprintf(stderr,"\nSending file now...\n");

	//while sending it is useful to make socket blocking as it will block until it is possible to send
	make_socket_blocking(clientfd);

	//First packet has header "File"

	while(total<filestatus.st_size){

		//find length of header

		if(filestatus.st_size-(total) > PACKET_SIZE-header_count-1 && total!=0){
			//	filestatus.st_size-total is the data remaining to be sent,
			// Remaining data > space in data packet, then this is an intermediate packet and not last packet.

			strcpy(file_head,"Pfil");
		}
		else if(filestatus.st_size-(total) <= PACKET_SIZE-header_count-1){
			//this is the last packet let the receiver know
			strcpy(file_head,"Endf");
		}

		//create file header appropriately
		sprintf((char *)data_buffer,"%4s %s %d \r",file_head,filename,(int) file_size);
		header_count=strlen((char *)data_buffer);
		//read in data in the buffer of quantity PACKET_SIZE-header_count-1, make space for \0
		count=fread (data_buffer+header_count , 1, PACKET_SIZE-header_count-1,File);
		total+=count;
		data_buffer[count+header_count]='\0';

		//send the data
		count=sendall(clientfd,data_buffer,count+header_count+1);

		if(count!=0){
			perror ("send");
			close(clientfd);
			return;
		}

	}

	//find end time
	clock_gettime(CLOCK_MONOTONIC, &tend);

	//find time difference
	double time=((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) -((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);

	//find file size in bits
	file_size=8*filestatus.st_size;
	fprintf(stderr,"\nFile %s sent successfully...\n",filename);

	//speed in bps
	double speed=(double) file_size/time;

	if (speed< 1024)
	{
		//speed in bps
		fprintf(stderr,"\nThe file uploaded at rate of %.2fbits per second...\n",speed);
	}
	else if (speed < 1048000)
	{
		//speed in Kbps
		speed= (double)speed/1024;
		fprintf(stderr,"\nThe file uploaded at rate of %.2fKbps...\n",speed);
	}
	else {
		//speed in Mbps
		speed= (double)speed/(1024*1024);
		fprintf(stderr,"\nThe file uploaded at rate of %.2fMbps...\n",speed);
	}

	fclose(File);
	make_socket_non_blocking(clientfd);
}

//Receive the file packet from given client.
void client_operations::recv_and_write_file(int clientfd, unsigned char* rem_buf){
	//size of the file
	int size;

	//count of the bytes received
	int count;

	//filename
	char filename[MAXMSGSIZE];

	//file size retried from the packet header
	char file_size[MAXMSGSIZE];

	//Buffer for the data
	unsigned char data_buf[PACKET_SIZE];

	//File pointer
	FILE* File;

	//is it last packet for this file
	bool last_packet=false;

	//Find time taken for download
	struct timespec tstart={0,0}, tend={0,0};

	//recieve packet from the client
	count = recv (clientfd, data_buf, PACKET_SIZE-initial_header_len, 0);
	data_buf[count-1]='\0';

	//Puts filename from the header to filename
	split_return((char *)(data_buf),' ',filename);

	//Puts file_size fromt the heade to file_size
	split_return((char *)(data_buf+strlen(filename)+1),' ',file_size);

	//convert char * size to int size
	size = strtoull(file_size, NULL, 0);

	if(!strcmp((char *)rem_buf,"File")){

		fprintf(stderr,"\nReceiving file now...\n");

		//if rem_buf is File that means this is first packet for this file
		//Therefore open file in write mode thus overwriting previous contents.

		File=fopen(filename,"wb");

		//Start the clock
		clock_gettime(CLOCK_MONOTONIC, &tstart);				//http://stackoverflow.com/questions/16275444/c-how-to-print-time-difference-in-accuracy-of-milliseconds-and-nanoseconds
		add_st_time(clientfd, (double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);
		//everything related to finding time difference I got from here^.
	}
	else if(!strcmp((char *)rem_buf,"Pfil")){
		//Otherwise the packet is any other packet than first packet thus file will be appended.
		File=fopen(filename,"ab");
	}

	else if(!strcmp((char *)rem_buf,"Endf")){
		File=fopen(filename,"ab");
		last_packet=true;
	}
	if(size==-1){
		//If size is -1(it's part of the protocol) then file was not found
		fprintf(stderr,"\nFile %s not found at the peer\n",filename);
		fclose(File);
		//handle any remaining downloads.
		handle_rem_downloads(clientfd);
		return;
	}

	if(count>0){
		//if count > 0 then write content to the file
		fwrite(data_buf+(return_first_occr((char *)data_buf,'\r')+1),1,count-4-strlen(filename)-strlen(file_size),File);
	}

	if(!last_packet){
		//if this is not the last packet exit.
		fclose(File);
		return;
	}

	else{
		//reached here means file download is complete
		clock_gettime(CLOCK_MONOTONIC, &tend);

		double time=((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) -(double)st_time(clientfd);

		//convert file size to bits
		size=8*size;
		fprintf(stderr,"\nFile %s received successfully...\n",filename);

		//find speed of transfer
		double speed= (double)size/time;

		if (speed< 1024)
		{
			//speed in bps
			fprintf(stderr,"\nThe file downloaded at rate of %.2fbits per second...\n",speed);
		}
		else if (speed < 1048000)
		{
			//speed in Kbps
			speed=(double) speed/1024;
			fprintf(stderr,"\nThe file downloaded at rate of %.2fKbps...\n",speed);
		}
		else {
			//speed in Mbps
			speed=(double) speed/(1024*1024);
			fprintf(stderr,"\nThe file downloaded at rate of %.2fMbps...\n",speed);
		}

		fclose (File);
		//handle any remaining downloads
		handle_rem_downloads(clientfd);
		return;
	}
}

//Requests from peers processed in this function
//some part is copied form http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
//and https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/
void client_operations::recv_requests_client(int clientfd){
	//token from buf
	char split_token[MAXMSGSIZE];
	int done = 0;

	ssize_t count;
	unsigned char buf[MAXMSGSIZE];

	//token array used for holding part of token
	char token_arr[MAXMSGSIZE];
	char* token=&token_arr[0];

	//read first 5 charcters to determine which type of packet is this
	count = recv (clientfd, buf, 5, 0);
	buf[count-1]='\0';
	if (count == -1)
	{
		/* If errno == EAGAIN, that means we have read all
				                         data. So go back to the main loop. */
		if (errno != EAGAIN)
		{
			perror ("read");

		}

	}
	else if (count == 0)
	{
		/* End of file. The remote has closed the
				                         connection. */
		//remove the clientfd from connected_list
		strcpy((char *)buf,remove_from_connected_list(clientfd, (char *)buf));
		fprintf(stderr,"\n%s closed the connection.\n",buf);
		close(clientfd);

	}
	else{

		if(buf){

			if (!strcmp((char *)buf,"Peer")){
				//peer list shared by the server
				char host_arr[MAXMSGSIZE];
				char ipstr[MAXMSGSIZE];
				char port[20];

				char *host=&host_arr[0];

				//receive remaining message
				count = recv (clientfd, buf, MAXMSGSIZE-5, 0);

				//token contains whole info for a given peer
				strcpy(token,strtok((char *)buf,"\n"));
				//host contains hostname of the peer
				host=strtok(token,"|");

				//reset peer_idx to zero.
				peer_idx=0;

				while(host!=NULL){
					//ip address
					strcpy(ipstr,strtok(NULL,"|"));
					strcpy(port,strtok(NULL,"|\n"));

					// put in peer_list all the info about the peer

					strcpy(peer_list.at(peer_idx).ipstr,ipstr);
					peer_list.at(peer_idx).port=new char[strlen(port)];
					strcpy(peer_list.at(peer_idx).port,port);
					peer_list.at(peer_idx).hostname=new char[strlen(host)];
					strcpy(peer_list.at(peer_idx).hostname,host);
					peer_list.at(peer_idx).file_descriptor=-1;
					peer_idx++;
					host=strtok(NULL,"|\n");
				}
			}

			else if (!strcmp((char *)buf,"File") || !strcmp((char *)buf,"Pfil") || !strcmp((char *)buf,"Endf")){
				//File packet received from the peer
				recv_and_write_file(clientfd,buf);
			}

			else if (!strcmp((char *)buf,"Send")){
				//Send command means other peer asking this peer to send a certain file, send command  may be triggered by download command on other peer

				count = recv (clientfd, buf, MAXMSGSIZE-5, 0);

				//file name of the file
				strcpy(split_token,strtok((char *)buf,"\n"));

				//send file to the peer
				send_file_over_socket(clientfd,split_token);
			}

			else {
				fprintf(stderr,"The server does not recognize %s command\n",token);

			}
		}
	}

}


//handle commands from the terminal
void inline client_operations::recv_stdin_client(int eventfd){
	//clientfd that may be will be retried in case of some commands
	int clientfd;

	//count returned by read etc.
	ssize_t count;

	//buf to receive data in
	char buf[MAXMSGSIZE];

	//send cmd buffer
	char send_cmd[MAXMSGSIZE];

	//capture first argument in this array
	char firstarg_arr[MAXMSGSIZE];
	//capture second argument in this array
	char secondarg_arr[MAXMSGSIZE];

	//using this pointer to point to the arrays in case strtok returns null.
	char *firstarg=firstarg_arr;
	char *secondarg=secondarg_arr;

	char token_arr[MAXMSGSIZE];
	char *token=token_arr;

	errno=0;


	count= read (0,buf,sizeof buf);

	if (count == -1)
	{
		/* If errno == EAGAIN, that means we have read all
			                         data. So go back to the main loop. */
		if (errno != EAGAIN)
		{
			perror ("read");

		}

	}
	else{
		if(buf!=NULL){
			//if buf not null
			token=strtok(buf," \r\n");
			//if token not null
			if(token!=NULL){

				//convert to uppercase
				server_operations::toupper(token);
				firstarg=strtok(NULL," \r\n");
				secondarg=strtok(NULL," \r\n");

				if (!strcmp(token,"HELP")){
					//help menu
					std::cout<<"Command Help"<<std::endl;
					std::cout<<"Help"<<std::setw(10)<<"Displays this help"<<std::endl;

					std::cout<<"MYIP"<<std::setw(10)<<"Display the IP address of this process."<<std::endl;
					std::cout<<"MYPORT"<<std::setw(10)<<"MYPORT Display the port on which this process is listening for incoming connections."<<std::endl;
					std::cout<<"REGISTER <server IP> <port_no>"<<std::setw(10)<<"Register the client to the server at timberlake at port_no ."<<std::endl;
					std::cout<<"CONNECT <destination> <port no>"<<std::setw(10)<<"Connect to the destination at port_no ."<<std::endl;
					std::cout<<"LIST"<<std::setw(10)<<"LIST all the available peers of the connection"<<std::endl;
					std::cout<<"TERMINATE <connection id>"<<std::setw(10)<<"Terminate the connection "<<std::endl;
					std::cout<<"EXIT"<<std::setw(10)<<"Exit the process"<<std::endl;
					std::cout<<"UPLOAD <connection id> <file name>"<<std::setw(10)<<"Upload file file_name to peer"<<std::endl;
					std::cout<<"DOWNLOAD <connection id 1> <file1>"<<std::setw(10)<<"Download file file_name from peer"<<std::endl;
					std::cout<<"CREATOR"<<std::setw(10)<<"Display creator's name and relevant info."<<std::endl;
				}

				else if (!strcmp(token,"REGISTER")){
					//Register to server

					if(firstarg!=NULL){
						//Register to server at the port specified by firstarg

						//connect to server
						int server_sock=connect_to_port(default_server,firstarg);
						if (server_sock>2){

							strcpy(send_cmd,"REGISTER ");
							strcat(send_cmd,server_port);
							strcat(send_cmd,"\n");

							//make socket non blocking
							make_socket_non_blocking(server_sock);
							make_entry_to_epoll(server_sock,eventfd);

							//send server request to register this peer
							count=sendall(server_sock,(unsigned char *)send_cmd,sizeof send_cmd);
							if(count!=0){
								perror ("send");
							}

						}

					}
					else{
						//error handling
						fprintf(stderr,"Invalid command; use help for command help\n");
					}
					return;
				}

				else if (!strcmp(token,"MYPORT")){
					//myport
					fprintf(stderr, "MY listening port is %s\n",server_port);

				}
				else if (!strcmp(token,"MYIP")){
					//myip
					strcpy(buf,my_ip(buf));
					if(strcmp(buf,"error")){
						fprintf(stderr, "MY IP is %s\n",buf);
					}
					else{
						//error handling
						fprintf(stderr, "Error occurred while retrieving IP\n");

					}
					return;
				}
				else if (!strcmp(token,"UPLOAD")){
					//upload command
					if (firstarg !=NULL && secondarg!=NULL){

						char *endptr;

						//convert firstarg to integer
						int connection_id = strtol(firstarg, &endptr, 10);									//taken this from man page of strtol
						if ((errno == ERANGE && (connection_id == INT_MAX || connection_id == INT_MIN))
								|| (errno != 0 && connection_id == 0)) {
							perror("strtol");

						}

						if (endptr == firstarg) {
							//error handling
							fprintf(stderr, "No digits were found\n");
							fprintf(stderr,"Invalid command use help for command help\n");

						}
						else{
							//make sure connection_id is valid
							if(connection_id<=connected_list_idx && connection_id!=1){
								clientfd=connected_list.at(connection_id-1).file_descriptor;
							}
							else{
								fprintf(stderr,"The connection id specified by you either does not exist or trying to download from server\n");
								return;
							}
							//send file to the peer
							send_file_over_socket(clientfd,secondarg);
						}
					}

					else{
						//error handling
						fprintf(stderr,"Invalid command use help for command help\n");
					}
					return;
				}

				else if (!strcmp(token,"DOWNLOAD")){
					//download command

					char *filename;

					if (firstarg !=NULL && secondarg!=NULL){
						redo_loop:		char *endptr;

						//convert firstarg to integer
						int connection_id = strtol(firstarg, &endptr, 10);
						if ((errno == ERANGE && (connection_id == INT_MAX || connection_id == INT_MIN))
								|| (errno != 0 && connection_id == 0)) {
							perror("strtol");

						}
						if (endptr == firstarg) {
							fprintf(stderr, "No digits were found\n");
							fprintf(stderr,"Invalid command use help for command help\n");

						}

						//get filename from secondarg
						filename=secondarg;


						if(connection_id<=connected_list_idx && connection_id!=1){
							//send request for file
							strcpy(send_cmd,"Send ");
							strcat(send_cmd,filename);
							strcat(send_cmd,"\n");

							//get the file desc. from the connection_id
							clientfd=connected_list.at(connection_id-1).file_descriptor;
							if(!is_download_on(clientfd)){
								//make sure no download with this peer is in progress

								send_download_command(clientfd,send_cmd);

							}
							else{
								//if download is in progress simply add the request to buffer and process later in recv_file function
								std::string str(send_cmd);
								send_cmd_buffer.push_back(str);

							}

							firstarg=strtok(NULL," \r\n");
							secondarg=strtok(NULL," \r\n");
							//see if any other download request is present
							if (firstarg !=NULL && secondarg!=NULL){
								goto redo_loop;
							}

						}
						else{
							//error handling
							fprintf(stderr,"The connection id specified by you either does not exist or trying to download from server\n");
							return;
						}

					}
					else{
						//error handling
						fprintf(stderr,"Invalid command use help for command help\n");


					}
					return;
				}
				else if (!strcmp(token,"CONNECT")){
					if(firstarg!=NULL && secondarg!=NULL){

						//connect to peer

						if(connected_list_idx==4){
							//Max limit for peers reached
							std::cout<<"\nReached limit of maximum connections terminate some connections first to add a new connection\n";
							return;
						}
						if (is_valid_peer(firstarg)){
							//make sure that peer is a valid peer

							//connect to peer
							clientfd= connect_to_port(firstarg,secondarg);

							if(clientfd <=2){
								return;
							}
							else {
								//make entry of the connection to clientfd
								make_entry_to_epoll(clientfd,eventfd);
								make_socket_non_blocking(clientfd);

							}
						}
						else{
							//error handling
							fprintf(stderr,"\nThe peer specified is not a valid peer \n");
						}
					}
					else{
						//error handling
						fprintf(stderr,"Invalid command use help for command help\n");
					}

					return;
				}

				else if (!strcmp(token,"TERMINATE")){
					if (firstarg!=NULL){
						//termiante the connection with the peer

						char *endptr;

						//convert firstarg to integer used strtol as it handles error better than atoi()
						int connection_id = strtol(firstarg, &endptr, 10);
						if ((errno == ERANGE && (connection_id == INT_MAX || connection_id == INT_MIN))
								|| (errno != 0 && connection_id == 0)) {
							perror("strtol");
							return;

						}
						if (endptr == firstarg) {
							fprintf(stderr, "No digits were found\n");
							std::cout<<"Invalid command use help for command help\n";
							return;
						}


						if(connection_id<=connected_list_idx && connection_id!=1){
							//if connection is valid and not server then termainte
							terminate_client(connection_id-1);

						}
						else{
							//error handling
							fprintf(stderr,"Connection id entered is either not valid or you trying to terminate connection with server\n");

						}
					}
					else{
						//error handling
						fprintf(stderr,"Invalid command use help for command help\n");


					}
					return;
				}

				else if (!strcmp(token,"LIST")){
					//print list of peers connected t the client
					std::cout<<"\nConnected peers are:\n";
					for (int i=0;i<connected_list_idx;i++){
						std::cout<<i+1<<"\t" <<connected_list.at(i).hostname<<"\t"<<connected_list.at(i).ipstr<< "\t\t" << connected_list.at(i).port<<"\n";

					}
					return;
				}

				else if(!strcmp(token,"EXIT")){
					//exit
					fprintf(stderr,"The Client is exiting..\n");
					exit(0);
				}

				else {
					//error handling
					fprintf(stderr,"Invalid command use help for command help\n");

				}
			}
		}
	}
}

//terminate the client if terminate command is used
void client_operations::terminate_client(int i){

	close(connected_list.at(i).file_descriptor);
	connected_list.at(i)=connected_list.at(--connected_list_idx);
	fprintf(stderr,"Terminated client at connection id %d\n",i+1);

}

//for the given peer find its port number from the peer_list, so that it can be displayed in LIST command.
char * client_operations::return_port_from_peer_list(int peer_fd){
	struct sockaddr_storage addr;
	socklen_t len = sizeof addr;
	char hostname[MAXMSGSIZE];
	char ipstr[INET6_ADDRSTRLEN];
	char service[20];
	getpeername(peer_fd, (struct sockaddr*)&addr, &len);
	if (addr.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;

		inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
	} else { // AF_INET6
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;

		inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
	}

	getnameinfo((struct sockaddr*)&addr,sizeof addr,hostname,sizeof hostname, service, sizeof service,0);

	for (int i=0;i<peer_idx;i++){

		//for some reason I keep getting some garbage along with hostname for highgate.cse.buffalo.edu e.g. highgate.cse.buffalo.edu1 or !highgate.cse.buffalo.edu
		//so added this strstr check to see if hostname in peer_list is contained within hostname.
		char *str=strstr(peer_list.at(i).hostname,hostname);

		if((!strcmp(peer_list.at(i).hostname,hostname)) || str){
			return peer_list.at(i).port;
		}
	}
	return NULL;
}

//wait for event
//similar to server function
void inline client_operations::wait_for_event(int eventfd, struct epoll_event* event_array, int sfd){
	struct sockaddr_storage in_addr;
	char port_arr[20];
	char *port=port_arr;
	while (1)
	{
		int n, i;
		int infd;
		n = epoll_wait (eventfd, event_array, maxPeers, -1);
		for (i = 0; i < n; i++)
		{

			if(event_array[i].events & EPOLLRDHUP){
				//***********
				char hostname [265];
				strcpy(hostname,remove_from_connected_list(event_array[i].data.fd,hostname));

				fprintf (stderr,"\nThe client %s closed connection",hostname);
				continue;

			}
			else if ((event_array[i].events & EPOLLERR) || (event_array[i].events & EPOLLHUP) || (!(event_array[i].events & EPOLLIN)))
			{
				/* An error has occured on this fd, or the socket is not
			                 ready for reading (why were we notified then?) */
				fprintf (stderr, "epoll error\n");
				close (event_array[i].data.fd);
				continue;
			}
			else if (sfd == event_array[i].data.fd)
			{
				/* We have a notification on the listening socket, which
			                 means one or more incoming connections. */
				socklen_t in_len;

				in_len = sizeof in_addr;
				infd = accept (sfd, (struct sockaddr *)&in_addr, &in_len);
				if (infd == -1)
				{
					perror ("accept");
					continue;
				}
				port=return_port_from_peer_list(infd);
				if(!port){
					fprintf (stderr, "Invalid connection rejected\n");
					close(infd);
					continue;
				}
				// add peer to connection_list
				add_connection_list(infd,port);

				//make entries
				make_socket_non_blocking(infd);
				make_entry_to_epoll(infd,eventfd);
				continue;

			}

			else if (event_array[i].data.fd==0){
				//handle stdin input
				recv_stdin_client(eventfd);

			}
			else
			{
				/* We have data on the fd waiting to be read.*/

				//handle the data from peers and server.
				recv_requests_client(event_array[i].data.fd);

			}
		}

	}
}
