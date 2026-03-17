/**
 * @assignment1
 * @author  Team Members <jacobmie@buffalo.edu, jehibber@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */


 #define POSIX_C_SOURCE 200809L 
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <sys/time.h>
 #include <netdb.h>
 #include <arpa/inet.h>
 #include <unistd.h>
 #include <stdbool.h> 
 #include <errno.h>      
 #include <ctype.h>      
 #define MAX_BUFFER 256         
 #define STDIN 0  //File descriptor for standard input
 #define MAX_CLIENTS 4 //max # of clients
 #define HOSTNAME_BUFFER_SIZE 256 // Buffer size for hostnames
 #define COMMAND_MAX_LEN 256  // max length of command
 #define LIST_BUFFER_SIZE 514 //buffer size for list response

 struct ClientInfo {
    int fd;                           // Socket file descriptor
    char ip_address_str[INET_ADDRSTRLEN]; // ip address
    char hostname[HOSTNAME_BUFFER_SIZE]; // Hostname
    int listening_port;             // Client's listening port 
    int logged_in_status;           // '1' = logged in
};

typedef struct {
  char buf[MAX_BUFFER];
  char ip_addr_str[INET_ADDRSTRLEN];
} Data ;

struct ClientState {
    fd_set *master_fds; // master file descriptor list (Ref: Beej 7.3)
    int *fdmax;         // maximum file descriptor number (Ref: Beej 7.3)
    int server_sock;    // server socket
    int logged_in;      // 1 = logged in
    char listening_port_str[10]; 
    char last_cmd[10]; 
};

struct ServerState {
    fd_set *master_fds;                 // master file descriptor list (Ref: Beej 7.3)
    int *fdmax;                         // maximum file descriptor number (Ref: Beej 7.3)
    struct ClientInfo clients[MAX_CLIENTS]; // current connected clients
    int num_clients;                    // count of the current connected clients
};

//function prototypes
void server(struct ServerState *server_state);
void client(struct ClientState *client_state);
int parse_args(int argc, char* argv[]);
void handle_command(struct ServerState *server_state, struct ClientState *client_state);
void *get_in_addr(struct sockaddr *sa); // Beej 6.1/7.3 helper
void add_client_record(struct ServerState *server_state, int new_client_fd, const char* ip, int client_listening_port);
void remove_client_record(struct ServerState *server_state, int client_fd);
int send_buffer(int fd, const char *buffer, int length);
int compare_clients_by_port(const void *port1, const void *port2);

//global variables
char *port;                          
char *run_mode; // 's' or 'c'
char *hostexternaladdr = NULL; //external ip address

int main(int argc, char **argv)
{
    //command line args
    if (!parse_args(argc,argv)) {
        return 1; // Exit on parse failure
    }

    struct ServerState server_state;
    struct ClientState client_state;

    client_state.server_sock = -1;
    client_state.logged_in = 0;
    server_state.num_clients = 0;

    hostexternaladdr = ""; // default val
    { 
        int status;
        char ipstr[INET_ADDRSTRLEN]; // buffer for ip
        struct addrinfo hints, *host = NULL; 
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;      // ipv4
        hints.ai_socktype = SOCK_DGRAM; // udp

	// BEGIN: 1) REFERENCE: https://ubmnc.wordpress.com/2010/09/22/on-getting-the-ip-name-of-a-machine-for-chatty/
        // google dns connection to get IP 
        if ((status = getaddrinfo("8.8.8.8", "53", &hints, &host)) != 0) {
            perror("Failed");
        } 
        else {
            int externsockfd;
            // create socket
            if ((externsockfd = socket(host->ai_family, host->ai_socktype, host->ai_protocol)) == -1) {
                perror("Failed");
            } 
            else {
                // Connect the UDP socket
                if (connect(externsockfd, host->ai_addr, host->ai_addrlen) == -1) {
                    perror("Failed");
                } 
                else {
                    socklen_t addrlen = sizeof(struct sockaddr_storage);
                    if (getsockname(externsockfd, host->ai_addr, &addrlen) == -1) {
                        perror("getsockname failed");
                    } 
                    else {
                        // extract address
                        struct sockaddr_in *ipv4 = (struct sockaddr_in *)host->ai_addr;
                        void *addr = &(ipv4->sin_addr);
                        inet_ntop(host->ai_family, addr, ipstr, sizeof ipstr); //formatted
                        hostexternaladdr = ipstr;
                    }
                }
                close(externsockfd); //close socket
            }
            if (host != NULL) {
                freeaddrinfo(host);
            }
        } // END OF REFERENCE 1
    } 

    // Copy listening port to client state
    strncpy(client_state.listening_port_str, port, sizeof(client_state.listening_port_str) - 1);
    client_state.listening_port_str[sizeof(client_state.listening_port_str) - 1] = '\0';

    if (strcmp(run_mode,"s") == 0) {
        server(&server_state);
    } 
    else {
        client(&client_state);
    }
    return 0; 
}

int parse_args(int argc, char* argv[]) {
    if (argc != 3) {
        perror("usage: ./assignment1 (c or s[c for client, s for server]), PORT to connect to");
        return 0;
    }
    if (strcmp(argv[1],"c") != 0 && strcmp(argv[1],"s") != 0) {
        perror("usage: ./assignment1 (c or s[c for client, s for server]), PORT to connect to");
        return 0;
    }
    for (char *port_char = argv[2]; *port_char; port_char++) { //port formatting
        if (!isdigit((unsigned char)*port_char)) {
            perror("must contain digits");
            return 0;
        }
    }
 
    int port_num = atoi(argv[2]);
    if (port_num <= 0 || port_num > 65535) {
        perror("failed");
        return 0;
    }

    port = argv[2];
    run_mode = argv[1];
    return 1;
}

void server(struct ServerState *state) {
  // reference 3) Server socket variables (Ref: Beej 7.3)
    //Master and temporary fd_sets for select
    fd_set master_fds; //master file descriptor list
    fd_set read_fds; //temp file descriptor list for select()
    int fdmax_val; //maximum file descriptor number

    state->master_fds = &master_fds;
    state->fdmax = &fdmax_val;

    // Server socket variables (Ref: Beej 7.3)
    int listener_fd;        // Listening socket descriptor
    int new_client_fd;      // Newly accepted connection descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;      //Client address length
    char recv_buf[MAX_BUFFER+INET_ADDRSTRLEN]; //Buffer for recv() check 
    int nbytes;            // Return val from recv()
    char remoteIP[INET_ADDRSTRLEN]; //clients ip string
    int yes=1;              //for setsockopt() SO_REUSEADDR
    int rv;                 // Generic return value checker
    struct addrinfo hints, *servinfo = NULL, *p; //initializing server info
    
    // END REFERENCE 3
    
    //START REFERENCE 4: beej 5.1 setup address info for the listening socket
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     
    hints.ai_socktype = SOCK_STREAM; //tcp
    hints.ai_flags = AI_PASSIVE;    

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        perror("error");
        exit(1);
    }
    // END REFERENCE 4

    
    // REFERENCE 5 Create listener socket and bind it (Beej 7.3 lines 61-82)
    listener_fd = -1; 
    for (p = servinfo; p != NULL; p = p->ai_next) {
        listener_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol); 
        if (listener_fd < 0) {
            continue;
        }
        setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        //Bind socket to the addrress
        if (bind(listener_fd, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener_fd); 
            listener_fd = -1; 
            continue;
        }
        break; //success
    }

    //check if binding failed for all the addresses
    if (listener_fd == -1) { 
        fprintf(stderr, "server: failed to bind socket\n");
        if (servinfo) { 
            freeaddrinfo(servinfo);
        } 
        exit(2);
    }
    //=free linked list
    if (servinfo) {
        freeaddrinfo(servinfo);
    } 

    //listen for incoming connections
    if (listen(listener_fd, MAX_CLIENTS) == -1) {
        perror("listen");
        close(listener_fd); // listen failure
        exit(3);
    }

    //Initialize master fd_set and max fd for select()
    FD_ZERO(state->master_fds);                
    FD_SET(listener_fd, state->master_fds);    
    FD_SET(STDIN, state->master_fds);          
    *(state->fdmax) = listener_fd;             

    // END REFERENCE 5
    
    printf("[SERVER]$: "); //prompt
    fflush(stdout);

    //main server loop
    for (;;) {
        read_fds = *(state->master_fds); 
        //waiting for activity
        if (select(*(state->fdmax) + 1, &read_fds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) {
                continue; //interrupted
            }
            perror("select"); 
            exit(4);
        }

        // check for any descriptors are ready
        for (int i = 0; i <= *(state->fdmax); i++) {
            if (FD_ISSET(i, &read_fds)) {
                //CHECk for any activity coming from stdin
                if (i == STDIN) {
                    handle_command(state, NULL); 
                } 
                //activity on the new connection
                else if (i == listener_fd) {
                    addrlen = sizeof remoteaddr;
                    new_client_fd = accept(listener_fd, (struct sockaddr *)&remoteaddr, &addrlen); //accept the new connection
                    if (new_client_fd == -1) {
                        perror("error");
                    } 
                    else {
                        // Check if server is full
                        if (state->num_clients > MAX_CLIENTS) {
                            perror("max clients reached");
                            close(new_client_fd);
                        } 
                        else {
                            // Read the LOGIN port
                            char login_buf[COMMAND_MAX_LEN];
                            nbytes = recv(new_client_fd, login_buf, sizeof(login_buf)-1, 0);
                            if (nbytes <= 0) {
                                if (nbytes < 0) {
                                    perror("error");
                                }
                                close(new_client_fd);
                            } 
                            else {
                                login_buf[nbytes] = '\0'; // Null-terminate
                                int client_listen_port = -1;
                                char command_check[10];
                                if (sscanf(login_buf, "%s %d", command_check, &client_listen_port) == 2 && strcmp(command_check, "LOGIN") == 0 && client_listen_port > 0 && client_listen_port <= 65535) {
                                    FD_SET(new_client_fd, state->master_fds); //add to master set
                                    if (new_client_fd > *(state->fdmax)) { //update fdmax
                                        *(state->fdmax) = new_client_fd;
                                    }
                                    inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, sizeof remoteIP);
                                    add_client_record(state, new_client_fd, remoteIP, client_listen_port); //add to list of client records
                                } 
                                else {
                                    close(new_client_fd);
                                }
                            }
                        }
                    }
                } 
                else {
		  nbytes = recv(i, recv_buf, sizeof recv_buf, MSG_DONTWAIT); //beejs 9.21 119
                    //handles disconnect
                    if (nbytes <= 0) {
                        if (nbytes < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
                            perror("recv peek check");
                        }
                        close(i);                     
                        FD_CLR(i, state->master_fds); //remvoe from set
                        remove_client_record(state, i);//remove from list
                    }
		    /*printf("\n[RELAYED SUCCESS]\n");
		  Data receivedbuf;
		  int destinationFD = -1;
		  int targetFD = -1;
		  int currFD = i;
		  // match bound socket with IP address

		  for(int curclient=0; curclient<MAX_CLIENTS;curclient++){
		    if (state->clients[curclient].fd == i){
		      destinationFD = curclient;
		      break;
		    }
		    //else{
		      //printf("Error something wrong\n");
		      //}
		  }

		  

		  //printf("CLIENTRECORDDEBUG:\nfdmax%d\nnumclients%d\n", *state->fdmax, state->num_clients);
		  //printf("client info: fd:%d\naddr:%s\nhostname:%s\nport:%d\n",
		  //	 state->clients[i].fd,
		  //	 state->clients[i].ip_address_str,state->clients[i].hostname
		  //	 , state->clients[i].listening_port);

		  
		  strncpy(receivedbuf.buf, recv_buf, nbytes);
		  receivedbuf.buf[nbytes] = '\0';

		  //printf("userdata.buf: %s\n", receivedbuf.buf);

		  //printf("client info2TESTTEST:%s\n", remoteIP);
		  // broadcast branch
		  strncpy(receivedbuf.ip_addr_str, recv_buf + MAX_BUFFER, INET_ADDRSTRLEN);
		  receivedbuf.ip_addr_str[INET_ADDRSTRLEN] = '\0';
		  if(strncmp(receivedbuf.ip_addr_str,"BROADCAST",sizeof "BROADCAST")==0){
		    //printf("maxfd -> %d\n", *(state->fdmax));
		    //printf("ddddddddddddddddddddddddddddddddddddddddddddddddddddddddd\n");
		    //printf("broadcast hit in server line 321\n");

		    // same stuff except to IP is 255.255.255.255, send to all connected sockfds except the server & itsself.
		    printf("msg from:%s, to %s\n", state->clients[destinationFD].ip_address_str,
			 "255.255.255.255");
		    // walk thru list, skip current fd and listenerfd
		    for(int i=0; i<state->num_clients; i++){
		      if(state->clients[i].fd == currFD){
			continue;
		      }
		      else{
			strncpy(receivedbuf.ip_addr_str,
			      state->clients[destinationFD].ip_address_str, INET_ADDRSTRLEN);
			send(state->clients[i].fd, &receivedbuf, sizeof receivedbuf, 0);
		      }
		  }
		    
		    memset(receivedbuf.buf, 0, sizeof(receivedbuf.buf));
		    printf("[RELAYED:END]\n");
		  }
		  else{
		  //printf("userdata.ip: %s\n", receivedbuf.ip_addr_str);

		  
		  //printf("DEBUG:IP: %s\n", state->clients[i].ip_address_str);

		  printf("DEBUG[SERVER]: nbytes: %d\n", nbytes);		  
		  printf("msg from:%s, to %s\n", state->clients[destinationFD].ip_address_str,
			 receivedbuf.ip_addr_str);
		  
		  printf("[msg]:%s\n", receivedbuf.buf);

		  for(int i=4;i< *(state->fdmax);i++){
		    if(strcmp(receivedbuf.ip_addr_str, state->clients[i].ip_address_str)==0){
		      targetFD = state->clients[i].fd;
		      strncpy(receivedbuf.ip_addr_str,
			      state->clients[destinationFD].ip_address_str, INET_ADDRSTRLEN);
		      send(targetFD, &receivedbuf, sizeof receivedbuf, 0);
		      break;
		    }
		    else{
		      //perror("ERROR: did not find a matching ip logged in\n");
		    }
		  }
		  printf("DEBUG[SERVER]: nbytes: %d\n", nbytes);
		  printf("[RELAYED:END]\n");
		  
		  */
                   //handles disconnect
                        // Read actual command
                        // [EXERPIMENTAL COMMENTED]nbytes = recv(i, recv_buf, sizeof(recv_buf) - 1, 0);
		    bool runmsg = true;
                        if (nbytes <= 0) {
                            if (nbytes < 0) {
                                perror("server: recv command");
                            }
                            close(i);
                            FD_CLR(i, state->master_fds);
                            remove_client_record(state, i);
                        }
                        else {
                            recv_buf[nbytes] = '\0';
			    
                            char command[MAX_BUFFER];
                            sscanf(recv_buf, "%s", command);
                            if (strcmp(command, "LIST") == 0 || strcmp(command, "REFRESH") == 0) {
			      runmsg = false;
                                char list_content_buffer[LIST_BUFFER_SIZE];
                                list_content_buffer[0] = '\0';
                                int logged_in_count = 0;
                                struct ClientInfo sorted_clients[MAX_CLIENTS];
                                for (int k = 0; k < state->num_clients; k++) {
                                    if (state->clients[k].logged_in_status == 1) {
                                        sorted_clients[logged_in_count++] = state->clients[k];
                                    }
                                }
                                qsort(sorted_clients, logged_in_count, sizeof(struct ClientInfo), compare_clients_by_port);
                                for (int k = 0; k < logged_in_count; k++) {
                                    struct ClientInfo *client = &sorted_clients[k];
                                    char cur_hostname[HOSTNAME_BUFFER_SIZE];
                                    char service[20];
                                    struct sockaddr_in clientinfo;
                                    memset(&clientinfo, 0, sizeof(clientinfo));
                                    clientinfo.sin_family = AF_INET;
                                    inet_pton(AF_INET, client->ip_address_str, &clientinfo.sin_addr);
                                    if (getnameinfo((struct sockaddr*)&clientinfo, sizeof(clientinfo), cur_hostname, sizeof(cur_hostname), service, sizeof(service), 0) != 0) {
                                        strncpy(cur_hostname, client->ip_address_str, sizeof(cur_hostname)-1);
                                    }
                                    cur_hostname[sizeof(cur_hostname)-1] = '\0';
                                    char temp_line[MAX_BUFFER * 2];
                                    sprintf(temp_line, "%-5d%-35s%-20s%-8d\n", k+1, cur_hostname, client->ip_address_str, client->listening_port);
                                    strcat(list_content_buffer, temp_line);
                                }
                                send_buffer(i, list_content_buffer, strlen(list_content_buffer));
                            } 
                            else if (strcmp(command, "LOGOUT") == 0) {
			      runmsg = false;
                                for (int k = 0; k < state->num_clients; k++) {
                                    if (state->clients[k].fd == i && state->clients[k].logged_in_status == 1) {
                                        state->clients[k].logged_in_status = 0;
                                        break;
                                    }
                                }
                            } 
                            else if (strcmp(command, "EXIT") == 0) {
			      runmsg = false;
                                close(i);
                                FD_CLR(i, state->master_fds);
                                remove_client_record(state, i);
                            } 
                        }
			// INSERT BROADCAST & MSSG LOGIC HERE
			if(runmsg){
			printf("\n[RELAYED SUCCESS]\n");
			printf("runmsg status: %b\n", runmsg);
			Data receivedbuf;
			int destinationFD = -1;
			int targetFD = -1;
			int currFD = i;
			// match bound socket with IP address

			for(int curclient=0; curclient<MAX_CLIENTS;curclient++){
			  if (state->clients[curclient].fd == i){
			    destinationFD = curclient;
			    break;
			  }
			  /* else{
			     printf("Error something wrong\n");
			     }*/
			}

		  

			//printf("CLIENTRECORDDEBUG:\nfdmax%d\nnumclients%d\n", *state->fdmax, state->num_clients);
			//printf("client info: fd:%d\naddr:%s\nhostname:%s\nport:%d\n",
			//	 state->clients[i].fd,
			//	 state->clients[i].ip_address_str,state->clients[i].hostname
			//	 , state->clients[i].listening_port);

		  
			strncpy(receivedbuf.buf, recv_buf, nbytes);
			receivedbuf.buf[nbytes] = '\0';

			//printf("userdata.buf: %s\n", receivedbuf.buf);

			//printf("client info2TESTTEST:%s\n", remoteIP);
			// broadcast branch
			strncpy(receivedbuf.ip_addr_str, recv_buf + MAX_BUFFER, INET_ADDRSTRLEN);
			receivedbuf.ip_addr_str[INET_ADDRSTRLEN] = '\0';
			if(strncmp(receivedbuf.ip_addr_str,"BROADCAST",sizeof "BROADCAST")==0){
			  //printf("maxfd -> %d\n", *(state->fdmax));
			  //printf("ddddddddddddddddddddddddddddddddddddddddddddddddddddddddd\n");
			  //printf("broadcast hit in server line 321\n");

			  // same stuff except to IP is 255.255.255.255, send to all connected sockfds except the server & itsself.
			  printf("msg from:%s, to %s\n", state->clients[destinationFD].ip_address_str,
				 "255.255.255.255");
			  printf("[msg]: %s\n",receivedbuf.buf);
			  // walk thru list, skip current fd and listenerfd
			  for(int i=0; i<state->num_clients; i++){
			    if(state->clients[i].fd == currFD){
			      continue;
			    }
			    else{
			      strncpy(receivedbuf.ip_addr_str,
				      state->clients[destinationFD].ip_address_str, INET_ADDRSTRLEN);
			      send(state->clients[i].fd, &receivedbuf, sizeof receivedbuf, 0);
			    }
			  }
		    
			  memset(receivedbuf.buf, 0, sizeof(receivedbuf.buf));
			  printf("[RELAYED:END]\n");
			}
			else{
			  //printf("userdata.ip: %s\n", receivedbuf.ip_addr_str);

			  printf("runmsg status[2]: %b\n", runmsg);
			  //printf("DEBUG:IP: %s\n", state->clients[i].ip_address_str);

			  printf("DEBUG[SERVER]: nbytes: %d\n", nbytes);		  
			  printf("msg from:%s, to %s\n", state->clients[destinationFD].ip_address_str,
				 receivedbuf.ip_addr_str);
		  
			  printf("[msg]:%s\n", receivedbuf.buf);

			  for(int i=4;i< *(state->fdmax);i++){
			    if(strcmp(receivedbuf.ip_addr_str, state->clients[i].ip_address_str)==0){
			      targetFD = state->clients[i].fd;
			      strncpy(receivedbuf.ip_addr_str,
				      state->clients[destinationFD].ip_address_str, INET_ADDRSTRLEN);
			      send(targetFD, &receivedbuf, sizeof receivedbuf, 0);
			      break;
			    }
			    else{
			      //perror("ERROR: did not find a matching ip logged in\n");
			    }
			  }
			  printf("DEBUG[SERVER]: nbytes: %d\n", nbytes);
			  printf("[RELAYED:END]\n");
			}
                    }
                }
            } 
        } 

        //reprint prompt
        if (FD_ISSET(STDIN, &read_fds)) {
            printf("[SERVER]$: ");
            fflush(stdout);
        }
    }
    close(listener_fd);
}

void client(struct ClientState *state) {
    // REFERNCE 6 Local fd_sets and max fd for client's select (beej 7.3)
    fd_set master_fds;
    fd_set read_fds;
    int fdmax_val;
  
    state->master_fds = &master_fds;
    state->fdmax = &fdmax_val;
  
    //client master fd set
    FD_ZERO(state->master_fds);       
    FD_SET(STDIN, state->master_fds); 
    *(state->fdmax) = STDIN;         
    // END REFERENCE 6
    printf("[CLIENT]$: "); //=promtpt
    fflush(stdout);
  
    //main loop of client
    for (;;) {
        read_fds = *(state->master_fds); //copy the master set
        if (select(*(state->fdmax) + 1, &read_fds, NULL, NULL, NULL) == -1) { //wait for activity, beej 7.3 select
            if (errno == EINTR) { //beej page 86, where it says Why does select() keep foalling out on a signal
                continue; 
            }
            perror("error");
            exit(4);
        }
  
        bool server_event = false;
        for (int i = 0; i <= *(state->fdmax); i++) { //check for descriptors that are ready
            if (FD_ISSET(i, &read_fds)) {
                if (i == STDIN) {
                    handle_command(NULL, state); //process the command
                } 
                else if (i == state->server_sock) { //found activity on server socket
		  server_event = true; 
		    char server_buf[MAX_BUFFER+INET_ADDRSTRLEN]; // previous
                    char server_listbuf[LIST_BUFFER_SIZE]; // experimental
		    //
		    printf("");
                  
                    int nbytes;
                    if ((nbytes = recv(state->server_sock, server_buf, sizeof(server_buf), MSG_DONTWAIT)) <= 0) { //from beejs 7.3 select
                        if (nbytes == 0) {
                            fprintf(stdout, "[CLIENT] Connection closed by server.\n");
                        } 
                        else { // Error
                            fprintf(stdout, "\n");
                            perror("client: recv");
                        }
                        close(state->server_sock);                   
                        FD_CLR(state->server_sock, state->master_fds); 
                        if (state->server_sock == *(state->fdmax)) { //if closed socket was max, find new fdmax
                            int new_max = STDIN;
                            for (int j = *(state->fdmax) - 1; j >= 0; j--) {
                                if (FD_ISSET(j, state->master_fds)) {
                                    new_max = j;
                                    break; // Found new max
                                }
                            }
                            *(state->fdmax) = new_max;
                        }
                        state->server_sock = -1;
                        state->logged_in = 0; //update state of the client
                        state->last_cmd[0] = '\0';

                    } 
                    else {
		      //if(nbytes == 272){ // if we are getting less than 256 & more than 1, not a msg/broadcast(272) & more than 272 it could be LIST
                        server_buf[nbytes] = '\0'; //null terminated
                      // inserted from newsend
			Data receivedbuf;
			char* cmd_name = state->last_cmd;

			//printf("IS LIST RUNNING THIS?\n");
			//printf("DEBUG[client]: nbytes: %d\n", nbytes);

                        strncpy(receivedbuf.buf, server_buf, MAX_BUFFER);
                        receivedbuf.buf[MAX_BUFFER] = '\0';

                        //printf("userdata.buf: %s\n", receivedbuf.buf);

                        //printf("client info2TESTTEST:%s\n", remoteIP);

                        strncpy(receivedbuf.ip_addr_str, server_buf + MAX_BUFFER, INET_ADDRSTRLEN);
                        receivedbuf.ip_addr_str[INET_ADDRSTRLEN] = '\0';
                        //printf("userdata.ip: %s\n", receivedbuf.ip_addr_str);

                        //printf("\n[RECEIVED:SUCCESS]\n");
                        //printf("msg from:%s\n", receivedbuf.ip_addr_str);
			fprintf(stdout, "\n[%s:SUCCESS]\n", cmd_name);
                        printf("\n%s\n", receivedbuf.buf);
                        //printf("[RECEIVED:END]\n");
                      //end of insertion
                      
                        //fprintf(stdout, "\n[CLIENT] Received %s\n", server_buf);
			//}
		       if (state->last_cmd[0] != '\0') {
			  server_listbuf[nbytes] = '\0';
			  
                            char* cmd_name = state->last_cmd;
                            //printf("\n");
                            //fprintf(stdout, "[%s:SUCCESS]\n", cmd_name);
                            //fprintf(stdout, "%s", server_listbuf);
                            if (server_listbuf[nbytes - 1] != '\n') {
			      //printf("\n");
                            }
                            fprintf(stdout, "[%s:END]\n", cmd_name);
			    //server_event = false;
                            state->last_cmd[0] = '\0';
			    //printf("IS LIST RUNNING THIS[2]?\n");
			    } 
			    else {
                            fprintf(stdout, "[CLIENT] Received %s\n", server_buf);
			    }
                    }
                }
            }
        }
        
        if (FD_ISSET(STDIN, &read_fds) || server_event) {
	  printf("[CLIENT]$: ");
	  fflush(stdout);
        }
    } // end client main loop
}

void handle_command(struct ServerState *server_state, struct ClientState *client_state) {
    char input_buffer[MAX_BUFFER];
    char cmd_line_copy[MAX_BUFFER];
    char *token;
  
    //end of file
    if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
        printf("[EXIT:SUCCESS]\n[EXIT:END]\n");
        if (client_state && client_state->server_sock != -1) {
            close(client_state->server_sock);
        }
        if (server_state && server_state->master_fds) {
            for (int k = 0; k < server_state->num_clients; k++) {
                if (server_state->clients[k].fd >= 0) {
                    close(server_state->clients[k].fd);
                }
            }
        }
        exit(0);
    }
    input_buffer[strcspn(input_buffer, "\n")] = 0; //remove newline
    if (strlen(input_buffer) == 0) { //for an empty line
        return;
    }
    strncpy(cmd_line_copy, input_buffer, sizeof(cmd_line_copy)-1);
    cmd_line_copy[sizeof(cmd_line_copy)-1] = '\0';
    token = strtok(cmd_line_copy, " ");


    //printf("input_buffer: %s\n", input_buffer);
  
    //whitespace lines
    if (!token) {
        return;
    }
    
    //COMMANDS 
    if (strcmp(token,"AUTHOR") == 0) {
        printf("[AUTHOR:SUCCESS]\nI, %s, have read and understood the course academic integrity policy.\n[AUTHOR:END]\n", "jacobmie-jehibber");
    }
    else if (strcmp(token,"IP")== 0) {
        if (hostexternaladdr && strlen(hostexternaladdr) > 0) { 
            printf("[IP:SUCCESS]\nIP:%s\n[IP:END]\n", hostexternaladdr);
        } 
        else { 
            printf("[IP:ERROR]\n[IP:END]\n");
        }
    }
    else if (strcmp(token,"PORT")== 0) {
        printf("[PORT:SUCCESS]\nPORT:%s\n[PORT:END]\n", port);
    }
    else if (strcmp(token, "EXIT") == 0) {
        printf("[EXIT:SUCCESS]\n[EXIT:END]\n");
        if (client_state && client_state->server_sock != -1) {
            send_buffer(client_state->server_sock, "EXIT\n", 5);
            close(client_state->server_sock);
        }
        if (server_state && server_state->master_fds) {
            for (int k = 0; k < server_state->num_clients; k++) {
                if (server_state->clients[k].fd >= 0) {
                    close(server_state->clients[k].fd);
                }
            }
        }
        exit(0); 
    }
    
    //SERVER SPECIFIC COMMANDS
    else if (strcmp(run_mode, "s") == 0 && server_state != NULL) {
      
        if (strcmp(token,"LIST") == 0) {
            printf("[LIST:SUCCESS]\n");
            int list_id = 1;

            int rv = 0;
            char cur_hostname[MAX_BUFFER];
            char service[20];
            struct ClientInfo sorted_clients[MAX_CLIENTS];
            int count = 0;
            for (int k = 0; k < server_state->num_clients; k++) {
                if (server_state->clients[k].logged_in_status == 1) {
                    sorted_clients[count++] = server_state->clients[k];
                }
            }
            qsort(sorted_clients, count, sizeof(struct ClientInfo), compare_clients_by_port);
            for (int k = 0; k < count; k++) {
                struct ClientInfo *client = &sorted_clients[k];
                struct sockaddr_in clientinfo;
                clientinfo.sin_family = AF_INET;
                clientinfo.sin_port = 0;
                struct in_addr clientaddr;
                inet_pton(AF_INET, client->ip_address_str, &clientaddr);
                clientinfo.sin_addr = clientaddr;
                rv = getnameinfo((struct sockaddr*)&clientinfo, sizeof(clientinfo), cur_hostname, sizeof(cur_hostname), service, sizeof(service), 0);
                if (rv != 0) {
                    perror("list hostname failed.\n");
                }
                printf("%-5d%-35s%-20s%-8d\n", list_id++, cur_hostname, client->ip_address_str, client->listening_port); 
            }
            printf("[LIST:END]\n");
        } 
        else {
            perror("unknown server command");
        }
    }
   
    //client specific commands
    else if (strcmp(run_mode, "c") == 0 && client_state != NULL) {

      
      // send command: check if first 4 bytes of token are "SEND", then ensure a VALID IP was typed (count the string length until whitespace,
      // next character after whitespace will be msg
      // S[0]END[3] 0.0.0.0||111.111.111.111<--WHITESPACE then msg buf length 256

      // ensure send command is not empty
      int whitespacecount = 0;
      bool sendfail = false;
      //printf("input buffer[2]:%s\n",input_buffer);
      for(int i=0; i < strlen(input_buffer); i++){
	if(input_buffer[i] == ' '){
	  whitespacecount++;
	}
      }
	
	

      //printf("whitespace count: %d\n", whitespacecount);
      if((strncmp(token,"SEND", 4) == 0) && whitespacecount < 2){
      sendfail = true;
      printf("[SEND:ERROR]\n");
      printf("[SEND:END]\n");
    }
      
      if((strncmp(token,"SEND", 4) == 0) && !sendfail) {
	printf("\n[SEND:SUCCESS]\n");
	char destip[INET_ADDRSTRLEN];
	int destipend = 0;
	struct in_addr checked_ip;
	for(int i=5; token[i] != ' '; i++){ // find the length user input string (IP)
	    destipend = i-5;
	}
	
	
	strncpy(destip,token+5,destipend+1); // copy the determined bytes into destip
	destip[destipend+1] = '\0'; // set last byte nul 

	if(inet_pton(AF_INET, destip, &checked_ip) != 1){ // check if IP is valid via inet_pton
	  printf("Invalid IP address.\n");
	  memset(&destip, 0, sizeof destip); // clear if invalid
	  printf("[SEND:END]\n");
	  return;
	}
	inet_ntop(AF_INET, &checked_ip, destip, INET_ADDRSTRLEN); // since inet_pton converts string into binary, use this function to turn binary back into a string.


	
	// send the message to server we don't care here if its logged in or valid. servers job.
	Data userdata;
	int offset = (5 + INET_ADDRSTRLEN);
	strncpy(userdata.buf, token + 7 + destipend, MAX_BUFFER);
	//printf("userdata.buf: %s\n", userdata.buf);
	strncpy(userdata.ip_addr_str, destip, INET_ADDRSTRLEN);
	//printf("userdata.ip: %s\n", userdata.ip_addr_str);
	//printf("[DEBUG]\nSIZEOF USERDATA: %ld\n[END DEBUG]\n", sizeof(userdata));
	send(client_state->server_sock, &userdata, sizeof userdata, 0);
	

	



	
	//1. Check with the logged in clients that the IP address is logged in(server)
	
	
	// if it is not ever logged in, invalid IP address. (server)
	//2. get the sockfd of the server, and of the client to send to
	//3. send the msg to the server, along with the ip
	//4. match the client ip with sockfd at server end.
	//5. check if the client is logged in or not
	//6. if the client is not logged in, we need to buffer.
	//7. buffer the message, and wait for the client to reestablish connection
	//8. send the message when the client has received communication.
	//9. (may be step 3, alert the client that the message was sent)
	//10. does the client need to know if the recipient is logged or not?
	//11. if so, give the alert. 


	
	//printf("<src-ip>: %s\n", destip); // remove for submission
	

	//memset(&destip, 0, sizeof destip);
	printf("[SEND:END]\n");
	}
      

      
      else if(strcmp(token,"BROADCAST") == 0){
	printf("\n[BROADCAST:SUCCESS]\n");
	char broadcast_alert[INET_ADDRSTRLEN];
	strncpy(broadcast_alert, "BROADCAST", sizeof "BROADCAST");
	broadcast_alert[INET_ADDRSTRLEN] = '\0';
	int destipend = 0;
	//printf("input_buffer: %s\n",input_buffer+sizeof "BROADCAST");
	struct in_addr checked_ip;
	for(int i=9; token[i] != ' '; i++){ // find the length user input string (IP)
	    destipend = i-9;
	}

	// send the message to server we don't care here if its logged in or valid. servers job.
	Data userdata;
	
	strncpy(userdata.buf, input_buffer+sizeof "BROADCAST", MAX_BUFFER);
	//printf("userdata.buf: %s\n", userdata.buf);
	strncpy(userdata.ip_addr_str, broadcast_alert, INET_ADDRSTRLEN);
	//printf("userdata.ip: %s\n", userdata.ip_addr_str);
	//printf("[DEBUG]\nSIZEOF USERDATA: %ld\n[END DEBUG]\n", sizeof(userdata));
	send(client_state->server_sock, &userdata, sizeof userdata, 0);
	
	
	//1. Check with the logged in clients that the IP address is logged in(server)	
	// if it is not ever logged in, invalid IP address. (server)
	//2. get the sockfd of the server, and of the client to send to
	//3. send the msg to the server, along with the ip
	//4. match the client ip with sockfd at server end.
	//5. check if the client is logged in or not
	//6. if the client is not logged in, we need to buffer.
	//7. buffer the message, and wait for the client to reestablish connection
	//8. send the message when the client has received communication.
	//9. (may be step 3, alert the client that the message was sent)
	//10. does the client need to know if the recipient is logged or not?
	//11. if so, give the alert. 	
	//printf("<src-ip>: %s\n", destip); // remove for submission
	//memset(&destip, 0, sizeof destip);
	printf("[BROADCAST:END]\n");
      }
	
		
      
        else if (strcmp(token,"LOGIN") == 0) {
            if (client_state->logged_in) { //check if already loged in
                printf("[LOGIN:ERROR]\nalready logged in\n[LOGIN:END]\n");
            } 
            else {
                //login args
                char *server_ip_str = strtok(NULL, " ");
                char *server_port_str = strtok(NULL, " ");
                char *extra_arg = strtok(NULL, " ");
                if (!server_ip_str || !server_port_str || extra_arg) {
                    printf("[LOGIN:ERROR]\nwrong usage\n[LOGIN:END]\n");
                } 
                else {
                    bool port_format_valid = true;
                    for (char *p = server_port_str; *p; p++) {
                        if (!isdigit((unsigned char)*p)) {
                            printf("[LOGIN:ERROR]\nwrong port format'%s'.\n[LOGIN:END]\n", server_port_str);
                            port_format_valid = false;
                            break; 
                        }
                    }
                    //port range
                    bool port_range_valid = false;
                    if (port_format_valid) {
                        int server_port_num = atoi(server_port_str);
                        if (server_port_num <= 0 || server_port_num > 65535) {
                            printf("[LOGIN:ERROR]\nout of range\n[LOGIN:END]\n");
                        } 
                        else {
                            port_range_valid = true;
                        }
                    }
                    if (port_range_valid) { 
                        // beej 6.2 client
                        int temp_sockfd = -1;
                        struct addrinfo login_hints, *servinfo_list = NULL, *p;
                        int rv;
                        memset(&login_hints, 0, sizeof login_hints);
                        login_hints.ai_family = AF_UNSPEC;      
                        login_hints.ai_socktype = SOCK_STREAM;  // TCP
                        // beej 5.1, resolve the server address
                        if ((rv = getaddrinfo(server_ip_str, server_port_str, &login_hints, &servinfo_list)) != 0) {
                            perror("failed to get server address");
                        } 
                        else {
                            //loops through results
                            for (p = servinfo_list; p != NULL; p = p->ai_next) {
                                //create socket
                                if ((temp_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                                    continue; 
                                }
                                //connect
                                if (connect(temp_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                                    close(temp_sockfd);      
                                    temp_sockfd = -1;        
                                    continue;                
                                }
                                break; //successful
                            }
                            // free allocated list
                            if (servinfo_list) {
                                freeaddrinfo(servinfo_list); 
                            }
                            // handles the connection
                            if (temp_sockfd == -1) { //error
                                printf("[LOGIN:ERROR]\nfailed to connect\n[LOGIN:END]\n");
                            } 
                            else { // Connected successfully
                                char login_msg[COMMAND_MAX_LEN];
                                sprintf(login_msg, "LOGIN %s\n", client_state->listening_port_str);
                                if (send_buffer(temp_sockfd, login_msg, strlen(login_msg)) == -1) {
                                    perror("client: send login");
                                    close(temp_sockfd);
                                } 
                                else {
                                    client_state->server_sock = temp_sockfd;
                                    client_state->logged_in = 1;
                                    FD_SET(client_state->server_sock, client_state->master_fds);
                                    if (client_state->server_sock > *(client_state->fdmax)) { //update fdmax
                                        *(client_state->fdmax) = client_state->server_sock;
                                    }
                                    printf("[LOGIN:SUCCESS]\n");
                                    printf("[LOGIN:END]\n");
                                }
                            } 
                        } 
                    } 
                }
            } 
        } 
        else if (strcmp(token,"LIST") == 0 || strcmp(token,"REFRESH") == 0) { //list command
            if (!client_state->logged_in) { //check if logged in (shouldnt work if not)
                printf("[LIST:ERROR]\nnot logged in\n[LIST:END]\n");
            } 
            else {
                char cmd_buf[10];
                sprintf(cmd_buf, "%s\n", token);
                if (send_buffer(client_state->server_sock, cmd_buf, strlen(cmd_buf)) == -1) {
                    perror("\nerror");
                } 
                else {
                    strncpy(client_state->last_cmd, token, sizeof(client_state->last_cmd)-1);
                    client_state->last_cmd[sizeof(client_state->last_cmd)-1] = '\0';
                }
            }
        } 
        else if (strcmp(token, "LOGOUT") == 0) {
            if (!client_state->logged_in) {
                printf("[LOGOUT:ERROR]\nYou are not logged in.\n[LOGOUT:END]\n");
            } 
            else {
                if (send_buffer(client_state->server_sock, "LOGOUT\n", 7) == -1) {
                    perror("\nerror");
                }
                close(client_state->server_sock);
                FD_CLR(client_state->server_sock, client_state->master_fds);
                if (client_state->server_sock == *(client_state->fdmax)) {
                    int new_max = STDIN;
                    for (int j = *(client_state->fdmax) - 1; j >= 0; j--) {
                        if (FD_ISSET(j, client_state->master_fds)) {
                            new_max = j;
                            break;
                        }
                    }
                    *(client_state->fdmax) = new_max;
                }
                client_state->server_sock = -1;
                client_state->logged_in = 0;
                client_state->last_cmd[0] = '\0';
                printf("[LOGOUT:SUCCESS]\n[LOGOUT:END]\n");
            }
        } 
      //else {
	// if(sendfail);
	   // perror("unknown client");
	   // }
    } 
    else {
        perror("error");
    }
}

//helper functions below 
void *get_in_addr(struct sockaddr *sa) { //beej helper function
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// helper method to add a new client record when a client connects to the server
void add_client_record(struct ServerState *server_state, int new_client_fd, const char* ip, int client_listening_port) {
    if (server_state->num_clients >= MAX_CLIENTS) { //check for full client list
        perror("max clients reached");
        close(new_client_fd);
        return;
    }
    int idx = server_state->num_clients;//adding client info
    server_state->clients[idx].fd = new_client_fd;
    strncpy(server_state->clients[idx].ip_address_str, ip, INET_ADDRSTRLEN-1);
    server_state->clients[idx].ip_address_str[INET_ADDRSTRLEN-1] = '\0'; //null terminated
    strncpy(server_state->clients[idx].hostname, ip, HOSTNAME_BUFFER_SIZE-1);
    server_state->clients[idx].hostname[HOSTNAME_BUFFER_SIZE-1] = '\0'; // nill terminated

    server_state->clients[idx].listening_port = client_listening_port;
    server_state->clients[idx].logged_in_status = 1;
    server_state->num_clients++;

    //new connection
    fprintf(stdout, "\n[SERVER] new connection: %s socket: %d number of clients connected: %d)\n", ip, new_client_fd, server_state->num_clients);
}

//remove client record from server (Client disconnection)
void remove_client_record(struct ServerState *server_state, int client_fd) {
    int i;
    char removed_ip[INET_ADDRSTRLEN] = "";
    //find client index
    for (i = 0; i < server_state->num_clients; i++) {
        if (server_state->clients[i].fd == client_fd) {
            strncpy(removed_ip, server_state->clients[i].ip_address_str, sizeof(removed_ip)-1);
            removed_ip[sizeof(removed_ip)-1] = '\0'; //null terminated
            break; 
        }
    }
    if (i == server_state->num_clients) {
        return;
    }
    fprintf(stdout, "\n[SERVER] connection closed: %s socket: %d\n", removed_ip, client_fd); //if client disconnects
    for (int j = i; j < server_state->num_clients - 1; j++) {
        server_state->clients[j] = server_state->clients[j+1];
    }
    server_state->num_clients--; //remove client
}

int send_buffer(int fd, const char *buffer, int length) {
    int total_sent = 0;
    int bytes_left = length;
    int n;
    while (total_sent < length) {
        n = send(fd, buffer + total_sent, bytes_left, 0);
        if (n == -1) {
            perror("failed");
            return -1;
        }
        total_sent += n;
        bytes_left -= n;
    }
    return 0;
}

int compare_clients_by_port(const void *port1, const void *port2) {
    const struct ClientInfo *clientA = (const struct ClientInfo*)port1;
    const struct ClientInfo *clientB = (const struct ClientInfo*)port2;
    if (clientA->listening_port < clientB->listening_port) {
        return -1;
    }
    if (clientA->listening_port > clientB->listening_port) {
        return 1;
    }
    return 0;
}
