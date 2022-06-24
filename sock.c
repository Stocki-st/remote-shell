// socket server TCP

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <string.h>

#include "shell.h"

#define PORT 5818
#define LISTENQ 20
#define BUFMAX 200


int main(int argc, char** argv){
    int print_output = 1;
     if (argc >= 1) {
        if (strcmp(argv[0], "no_output") == 0) {
            print_output = 0;
            fclose(stderr);
        }
     }

    int sockfd, clientfd;
    struct sockaddr_in clientaddr, serveraddr;
    socklen_t clientaddrlen;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        perror("create socket");
        exit(1);
    }

    // setsockopt() free previously used sockets()
    int reuseaddr = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1) {
        perror("setsockopt error");
    }
    //preparation of the socket address
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(PORT);


    if(-1 == bind (sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr))) {
        perror("socket bind");
        exit(1);
    }

    if (listen(sockfd, LISTENQ) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for(;;) {

        clientfd = accept(sockfd,  (struct sockaddr *) &clientaddr, &clientaddrlen);
        if(clientfd == -1) {
            perror("accept");
            exit(3);
        }
        
        if(print_output){
            printf("Client connected...\n");
        }
        pid_t clientd_handler = fork();
        if(clientd_handler < 0) {
            perror("fork");
            continue;
        } else if(!clientd_handler) {
            shell(0,NULL, clientfd); // start shell
            return 1;
        } else {
            // parent does nothing
            close(clientfd); // fd not needed anymore 
        }
    }
    return 0;
}
