/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define TRUE   1
#define FALSE  0
#define ERROE -1

#define PORT "3500"  // the port users will be connecting to
#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 1000

typedef struct{
	char field[MAXDATASIZE];
	int numbytes;
}Message;

int exten(Message *cmd);

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	Message cmd;
	Message name;
	Message output;
	int pid;
	int in[2], out[2];
	int status, i, event, total_bytes_read;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
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

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			close(1);
			dup2(new_fd, 1);
			do{
				cmd.numbytes=recv(new_fd, cmd.field, MAXDATASIZE-1, 0);
				cmd.field[cmd.numbytes]='\0';
				if(exten(&cmd)){
					name.numbytes=recv(new_fd, name.field, MAXDATASIZE-1, 0);
					name.field[name.numbytes]='\0';
				}if(!strcmp(cmd.field, "list")){
					pipe(in);
					pipe(out);
					pid=fork();
					if(pid==0){
						// child process
						close(0);
						close(1);
						close(2);
						dup2(in[0], 0);
						dup2(out[1], 1);
						dup2(out[1], 2);
						close(in[1]);
						close(out[0]);
						execl("/bin/ls", "/bin/ls", (char *)NULL);
					}else{
						// parent process
						close(in[0]);
						close(out[1]);
						close(in[1]);
						wait(&status);
						output.numbytes=read(out[0], output.field, MAXDATASIZE);
						output.field[output.numbytes]='\0';
						printf("%s", output.field);
					}
				}else if(!strcmp(cmd.field, "check")){
					pipe(in);
					pipe(out);
					pid=fork();
					if(pid==0){
						// child process
						close(0);
						close(1);
						close(2);
						dup2(in[0], 0);
						dup2(out[1], 1);
						dup2(out[1], 2);
						close(in[1]);
						close(out[0]);
						execl("/bin/find", "/bin/find", name.field, (char *)NULL);
					}else{
						// parent process
						close(in[0]);
						close(out[1]);
						close(in[1]);
						wait(&status);
						output.numbytes=read(out[0], output.field, MAXDATASIZE);
						output.field[output.numbytes]='\0';
						event=TRUE;
						for(i=0; output.field[i]!='\0'; i++){
							if(output.field[i]==' '){
								printf("file not found\n");
								event=FALSE;
								break;
							}
						}if(event){
							for(i=1; output.field[i]!='\0'; i++){
								putchar(output.field[i-1]);
							}printf(" found\n", output.field);
						}
					}
				}else if(!strcmp(cmd.field, "display")){
					pipe(in);
					pipe(out);
					pid=fork();
					if(pid==0){
						// child process
						close(0);
						close(1);
						close(2);
						dup2(in[0], 0);
						dup2(out[1], 1);
						dup2(out[1], 2);
						close(in[1]);
						close(out[0]);
						execl("/bin/cat", "/bin/find", name.field, (char *)NULL);
					}else{
						// parent process
						close(in[0]);
						close(out[1]);
						close(in[1]);
						wait(&status);
						total_bytes_read=0;
						
						do{
						output.numbytes=read(out[0], output.field+total_bytes_read, MAXDATASIZE);
						if(output.numbytes!=MAXDATASIZE) output.field[output.numbytes]='\0';
						printf("%s", output.field);
						total_bytes_read+=output.numbytes;
						}while(output.numbytes==MAXDATASIZE);
					}
				}else if(!strcmp(cmd.field, "download")){
					printf("function download missing\n");
				}else if(!strcmp(cmd.field, "help")){
					printf("function help missing\n");
				}
			}while(strcmp(cmd.field, "quit"));
			close(new_fd);
			printf("connection closed\n");
			exit(0);
		}close(new_fd);  // parent doesn't need this
	}return 0;
}

int exten(Message *cmd){
	int i;
	i=!strcmp(cmd->field, "check");
	i=i+!strcmp(cmd->field, "display");
	i=i+!strcmp(cmd->field, "download");
	return i;
}
