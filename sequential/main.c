#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "utils.h"

int main(int argc, char** argv) {
    int portnum = 3000;
    if (argc >= 2) {
        portnum = atoi(argv[1]);
    }
    printf("Listening on port %d\n", portnum);

    int sockfd = listen_socket(portnum);

    while (1) {
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);

        int newsockfd = accept(sockfd, (struct sockaddr*)&peer_addr, peer_addr_len);
        
        log_peer_connection(&peer_addr, peer_addr_len);
    }


    return EXIT_SUCCESS;
}
