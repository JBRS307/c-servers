#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "utils.h"

#define N_BACKLOG 32

void perror_die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int listen_socket(int portnum) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror_die("ERROR opening socket");
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt)) < 0) {
        perror_die("setsockopt");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(portnum);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror_die("ERROR binding");
    }

    if (listen(sockfd, N_BACKLOG) < 0) {
        perror_die("ERROR listen");
    }
}

void log_peer_connection(const struct sockaddr_in* sa, socklen_t sa_len) {
    char hostbuf[NI_MAXHOST];
    char portbuf[NI_MAXSERV];
    if (getnameinfo((struct sockaddr*)sa, sa_len, hostbuf, NI_MAXHOST, portbuf, NI_MAXSERV, 0) == 0) {
        printf("peer (%s, %s) connected\n", hostbuf, portbuf);
    } else {
        printf("peer (unknown) connected\n");
    }
}