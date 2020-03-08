#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>


#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define RESET "\033[0m"

#define LOGIN_SIZE 32
#define MESSAGE_SIZE 256

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


typedef struct clientInfo{
	int socket;
	struct sockaddr_in address;
	char username[LOGIN_SIZE];
}clientInfo;


typedef struct{
	message_type type;
	char username[LOGIN_SIZE];
	char data[MESSAGE_SIZE];
}message;


// get a username from the user.
void getUsername(char *username){
	while(true){
		printf("Introduceti un nume de utilizator: ");
		fflush(stdout);
		memset(username, 0, 1000);
		fgets(username, LOGIN_SIZE, stdin);
        int temp = strlen(username) - 1;
        if(username[temp] == '\n')
            username[temp] = '\0';

		if(strlen(username) > LOGIN_SIZE){
			puts("Numele de utilizator trebuie sa fie de cel mult 32 caractere");
		}
		else{
			break;
		}
	}
}

//send local username to the server.
void setUsername(clientInfo *connection){
	message msg;
	msg.type = SET_USERNAME;
	strncpy(msg.username, connection->username, LOGIN_SIZE);

	if(send(connection->socket, (void*)&msg, sizeof(msg), 0) < 0){
		perror("Trimitere esuata..");
		exit(1);
	}
}

void killClient(clientInfo *connection){
	close(connection->socket);
	exit(0);
}

//initialize connection to the server.
void connect(clientInfo *connection, char *address, char *port){

	while(true){
		getUsername(connection->username);

		//Create socket
		if((connection->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
			perror("Eroare creare socket..");
		}

		connection->address.sin_addr.s_addr = inet_addr(address);
		connection->address.sin_family = AF_INET;
		connection->address.sin_port = htons(atoi(port));

		//Connect to remote server
		if(connect(connection->socket, (struct sockaddr *)&connection->address, sizeof(connection->address)) < 0){
			perror("Conexiune esuata..");
			exit(1);
		}

		setUsername(connection);

		message msg;
		ssize_t recv_val = recv(connection->socket, &msg, sizeof(message), 0);
		if(recv_val < 0){
			perror("Primire mesaj esuata..");
			exit(1);

		}
		else if(recv_val == 0){
			close(connection->socket);
			printf("Utilizatorul \"%s\" este deja luat,incercati altul.\n", connection->username);
			continue;
		}
		break;
	}
	puts("Conectat la server.WOO!");
	puts("Tasteaza /H pentru mai multe optiuni..");
}


void userOptions(clientInfo *connection){
	char input[MESSAGE_SIZE];
	fgets(input, MESSAGE_SIZE, stdin);
    int temp = strlen(input)-1;
    if(input[temp] == '\n'){
        input[temp] = '\0';
    }
    
	if(strcmp(input, "/Q") == 0){
		killClient(connection);
	}
	else if(strcmp(input, "/L") == 0){
		message msg;
		msg.type = GET_USERS;

		if(send(connection->socket, &msg, sizeof(message), 0) < 0){
			perror("Trimitere esuata..");
			exit(1);
		}
	}
	else if(strcmp(input, "/H") == 0){
		puts("/Q: Paraseste chat-ul.");
		puts("/H: Afiseaza informatii utile.");
		puts("/L: Afiseaza utilizatori logati la chat.");
		puts("/PM: <utilizator> <mesaj> Trimite mesaj privat utilizatorului.");
	}
	else if(strncmp(input, "/PM", 3) == 0){
		message msg;
		msg.type = PRIVATE_MESSAGE;

		char *toUsername, *chatMsg;

		toUsername = strtok(input + 3, " ");

		if(toUsername == NULL){
			puts(KRED "Incearca  /PM <utilizator> <mesaj>" RESET);
			return;
		}

		if(strlen(toUsername) == 0){
			puts(KRED "Incearca sa trimiti cuiva anume.(Nu uita formatul)" RESET);
			return;
		}

		if(strlen(toUsername) > LOGIN_SIZE){
			puts(KRED "Numele utilizatorului intre 1 si 32 litere" RESET);
			return;
		}

		chatMsg = strtok(NULL, "");

		if(chatMsg == NULL){
			puts(KRED "Daca nu vrei sa-i spui nimic nu mai folosi chat-ul." RESET);
			return;
		}

		//printf("|%s|%s|\n", toUsername, chatMsg);
		strncpy(msg.username, toUsername, LOGIN_SIZE);
		strncpy(msg.data, chatMsg, MESSAGE_SIZE);

		if(send(connection->socket, &msg, sizeof(message), 0) < 0){
			perror("Trimitere esuata..");
			exit(1);
		}
	}
	else{ //regular public message
		message msg;
		msg.type = PUBLIC_MESSAGE;
		strncpy(msg.username, connection->username, LOGIN_SIZE);

		if(strlen(input) == 0){
			return;
		}

		strncpy(msg.data, input, MESSAGE_SIZE);

		//Send some data
		if(send(connection->socket, &msg, sizeof(message), 0) < 0){
			perror("Trimitere esuata..");
			exit(1);
		}
	}
}

void serverMessages(clientInfo *connection){
	message msg;

	//Receive a reply from the server
	ssize_t recv_val = recv(connection->socket, &msg, sizeof(message), 0);
	if(recv_val < 0){
		perror("Primire raspuns esuata..");
		exit(1);
	}
	else if(recv_val == 0){
		close(connection->socket);
		puts("Server deconectat..");
		exit(0);
	}

	switch(msg.type){

	case CONNECT:
		printf(KYEL "%s s-a alaturat!!" RESET "\n", msg.username);
		break;

	case DISCONNECT:
		printf(KYEL "%s s-a deconectat!" RESET "\n", msg.username);
		break;

	case GET_USERS:
		printf("%s", msg.data);
		break;

	case SET_USERNAME:
		//TODO: implement: name changes in the future?
		break;

	case PUBLIC_MESSAGE:
		printf(KGRN "%s" RESET ": %s\n", msg.username, msg.data);
		break;

	case PRIVATE_MESSAGE:
		printf(KWHT "De la %s:" KCYN " %s\n" RESET, msg.username, msg.data);
		break;

	case TOO_FULL:
		fprintf(stderr, KRED "Chat plin, incearca mai tarziu.." RESET "\n");
		exit(0);
		break;

	default:
		fprintf(stderr, KRED "Mesaj necunoscut.." RESET "\n");
		break;
	}
}

int main(int argc, char *argv[]){
	clientInfo connection;
	fd_set file_descriptors;

	if(argc != 3){
		fprintf(stderr, "Incearca: %s <IP> <port>\n", argv[0]);
		exit(1);
	}

	connect(&connection, argv[1], argv[2]);

	//keep communicating with server
	while(true){
		FD_ZERO(&file_descriptors);
		FD_SET(STDIN_FILENO, &file_descriptors);
		FD_SET(connection.socket, &file_descriptors);
		fflush(stdin);

		if(select(connection.socket + 1, &file_descriptors, NULL, NULL, NULL) < 0){
			perror("Selectare esuata..");
			exit(1);
		}

		if(FD_ISSET(STDIN_FILENO, &file_descriptors)){
			userOptions(&connection);
		}

		if(FD_ISSET(connection.socket, &file_descriptors)){
			serverMessages(&connection);
		}
	}
	close(connection.socket);
	return 0;
}
