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


#define PORT 5818
#define LISTENQ 20
#define BUFMAX 200


int main() {

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

        printf("Client connected...\n");
        FILE* fp;
        char buf[BUFMAX];

        fp = fdopen(clientfd,"w+");

        //handle clients (e.g. with own thread)
        fprintf(fp, "Hallo du!\n");
        fflush(fp);

        fgets(buf, BUFMAX,fp);
        printf("%s",buf);
        fclose(fp);
    }


    return 0;
}
