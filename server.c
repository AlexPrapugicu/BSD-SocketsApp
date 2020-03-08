#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "chatroom_utils.h"

#define MAX_CLIENTS 4



//Enum of different messages possible.
typedef enum{
	CONNECT,
	DISCONNECT,
	GET_USERS,
	SET_USERNAME,
	PUBLIC_MESSAGE,
	PRIVATE_MESSAGE,
	TOO_FULL,
	USERNAME_ERROR,
	SUCCESS,
	ERROR
} message_type;


//message structure
typedef struct{
	message_type type;
	char username[21];
	char data[256];
}message;

//structure to hold client connection information
typedef struct clientInfo{
	int socket;
	struct sockaddr_in address;
	char username[20];
}clientInfo;


void serverInit(clientInfo *server, int port){
	if((server->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Failed to create socket");
		exit(1);
	}

	server->address.sin_family = AF_INET;
	server->address.sin_addr.s_addr = INADDR_ANY;
	server->address.sin_port = htons(port);

	if(bind(server->socket, (struct sockaddr *)&server->address, sizeof(server->address)) < 0){
		perror("Binding failed");
		exit(1);
	}

	const int optVal = 1;
	const socklen_t optLen = sizeof(optVal);
	if(setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, (void*)&optVal, optLen) < 0){
		perror("Set socket option failed");
		exit(1);
	}

	if(listen(server->socket, 3) < 0){
		perror("Listen failed");
		exit(1);
	}

	//Accept and incoming connection
	printf("Waiting for connections ..\n");
}
// revised ok
void publicMessage(clientInfo clients[], int sender, char *message_text){
	message msg;
	msg.type = PUBLIC_MESSAGE;
	strncpy(msg.username, clients[sender].username, 20);
	strncpy(msg.data, message_text, 256);
	int i = 0;
	for(i = 0; i < MAX_CLIENTS; i++){
		if(i != sender && clients[i].socket != 0){
			if(send(clients[i].socket, &msg, sizeof(msg), 0) < 0){
				perror("Send failed");
				exit(1);
			}
		}
	}
}
// revised ok 
void privateMessage(clientInfo clients[], int sender,char *username, char *message_text){
	message msg;
	msg.type = PRIVATE_MESSAGE;
	strncpy(msg.username, clients[sender].username, 20);
	strncpy(msg.data, message_text, 256);

	int i;
	for(i = 0; i < MAX_CLIENTS; i++){
		if(i != sender && clients[i].socket != 0 && strcmp(clients[i].username, username) == 0){
			if(send(clients[i].socket, &msg, sizeof(msg), 0) < 0){
				perror("Send failed");
				exit(1);
			}
			return;
		}
	}

	msg.type = USERNAME_ERROR;
	sprintf(msg.data, "Username \"%s\" does not exist or is not logged in.", username);

	if(send(clients[sender].socket, &msg, sizeof(msg), 0) < 0){
		perror("Send failed");
		exit(1);
	}

}
// revised ok
void userConnectNotify(clientInfo *clients, int sender){
	message msg;
	msg.type = CONNECT;
	strncpy(msg.username, clients[sender].username, 21);
	int i = 0;
	for(i = 0; i < MAX_CLIENTS; i++){
		if(clients[i].socket != 0){
			if(i == sender){
				msg.type = SUCCESS;
				if(send(clients[i].socket, &msg, sizeof(msg), 0) < 0){
					perror("Send failed");
					exit(1);
				}
			}
			else{
				if(send(clients[i].socket, &msg, sizeof(msg), 0) < 0){
					perror("Send failed");
					exit(1);
				}
			}
		}
	}
}
//revised ok
void userDisconnectNotify(clientInfo *clients, char *username){
	message msg;
	msg.type = DISCONNECT;
	strncpy(msg.username, username, 21);
	int i = 0;
	for(i = 0; i < MAX_CLIENTS; i++){
		if(clients[i].socket != 0){
			if(send(clients[i].socket, &msg, sizeof(msg), 0) < 0){
				perror("Send failed");
				exit(1);
			}
		}
	}
}
// revised ok
void sendUserList(clientInfo *clients, int receiver){
	message msg;
	msg.type = GET_USERS;
	char *list = msg.data;

	int i;
	for(i = 0; i < MAX_CLIENTS; i++){
		if(clients[i].socket != 0){
			list = stpcpy(list, clients[i].username);
			list = stpcpy(list, "\n");
		}
	}

	if(send(clients[receiver].socket, &msg, sizeof(msg), 0) < 0){
		perror("Send failed");
		exit(1);
	}
}

//revised ok
void chatFullNotify(int socket){
	message fullMsg;
	fullMsg.type = TOO_FULL;

	if(send(socket, &fullMsg, sizeof(fullMsg), 0) < 0){
		perror("Send failed");
		exit(1);
	}
	close(socket);
}

//revised ok
//close all the sockets before exiting
void stopServer(clientInfo connection[]){
	int i;
	for(i = 0; i < MAX_CLIENTS; i++){
		close(connection[i].socket);
	}
	exit(0);
}


void handle_client_message(clientInfo clients[], int sender){
	int read_size;
	message msg;

	if((read_size = recv(clients[sender].socket, &msg, sizeof(message), 0)) == 0){
		printf("User disconnected: %s.\n", clients[sender].username);
		close(clients[sender].socket);
		clients[sender].socket = 0;
		userDisconnectNotify(clients, clients[sender].username);

	}
	else{

		switch(msg.type){
		case GET_USERS:
			sendUserList(clients, sender);
			break;

		case SET_USERNAME:;
			int i;
			for(i = 0; i < MAX_CLIENTS; i++){
				if(clients[i].socket != 0 && strcmp(clients[i].username, msg.username) == 0){
					close(clients[sender].socket);
					clients[sender].socket = 0;
					return;
				}
			}

			strcpy(clients[sender].username, msg.username);
			printf("User connected: %s\n", clients[sender].username);
			userConnectNotify(clients, sender);
			break;

		case PUBLIC_MESSAGE:
			publicMessage(clients, sender, msg.data);
			break;

		case PRIVATE_MESSAGE:
			privateMessage(clients, sender, msg.username, msg.data);
			break;

		default:
			fprintf(stderr, "Unknown message type received.\n");
			break;
		}
	}
}

int construct_fd_set(fd_set *set, clientInfo *server_info,clientInfo clients[]){
	FD_ZERO(set);
	FD_SET(STDIN_FILENO, set);
	FD_SET(server_info->socket, set);

	int max_fd = server_info->socket;
	int i;
	for(i = 0; i < MAX_CLIENTS; i++){
		if(clients[i].socket > 0){
			FD_SET(clients[i].socket, set);
			if(clients[i].socket > max_fd){
				max_fd = clients[i].socket;
			}
		}
	}
	return max_fd;
}
//revised
void handle_new_connection(clientInfo *server_info, clientInfo clients[]){
	int newSocket;
	int address_len;
	newSocket = accept(server_info->socket, (struct sockaddr*)&server_info->address, (socklen_t*)&address_len);

	if(newSocket < 0){
		perror("Accept Failed");
		exit(1);
	}

	int i;
	for(i = 0; i < MAX_CLIENTS; i++){
		if(clients[i].socket == 0){
			clients[i].socket = newSocket;
			break;
		}
		else if(i == MAX_CLIENTS - 1){ // if we can accept no more clients{
			chatFullNotify(newSocket);
		}
	}
}

void handle_user_input(clientInfo clients[])
{
	char input[255];
	fgets(input, sizeof(input), stdin);
	trim_newline(input);

	if(input[0] == 'q') {
		stopServer(clients);
	}
}

int main(int argc, char *argv[])
{
	puts("Starting server.");

	fd_set file_descriptors;

	clientInfo server_info;
	clientInfo clients[MAX_CLIENTS];

	int i;
	for(i = 0; i < MAX_CLIENTS; i++){
		clients[i].socket = 0;
	}

	if(argc != 2){
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(1);
	}

	serverInit(&server_info, atoi(argv[1]));

	while(true){
		int max_fd = construct_fd_set(&file_descriptors, &server_info, clients);

		if(select(max_fd + 1, &file_descriptors, NULL, NULL, NULL) < 0){
			perror("Select Failed");
			stopServer(clients);
		}

		if(FD_ISSET(STDIN_FILENO, &file_descriptors)){
			handle_user_input(clients);
		}

		if(FD_ISSET(server_info.socket, &file_descriptors)){
			handle_new_connection(&server_info, clients);
		}

		for(i = 0; i < MAX_CLIENTS; i++){
			if (clients[i].socket > 0 && FD_ISSET(clients[i].socket, &file_descriptors)){
				handle_client_message(clients, i);
			}
		}
	}
	return 0;
}