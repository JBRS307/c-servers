#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils.h"

void serve_connection(int sockfd);

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

        int newsockfd = accept(sockfd, (struct sockaddr*)&peer_addr, &peer_addr_len);
        
        log_peer_connection(&peer_addr, peer_addr_len);
        serve_connection(newsockfd);
    }


    return EXIT_SUCCESS;
}

typedef enum processing_state {
    WAIT_FOR_MSG,
    IN_MSG,
} ProcessingState;

void serve_connection(int sockfd) {
    if (send(sockfd, "*", 1, 0) < 0) {
        perror_die("ERROR send");
    }

    ProcessingState state = WAIT_FOR_MSG;

    while (1) {
        uint8_t buf[1024];
        int len = recv(sockfd, buf, sizeof(buf), 0);
        if (len < 0) {
            perror_die("ERROR recv");
        } else if (len == 0) {
            break;
        }

        for (int i = 0; i < len; i++) {
            switch (state) {
                case WAIT_FOR_MSG:
                    if (buf[i] == '^') {
                        state = IN_MSG;
                    }
                    break;
                case IN_MSG:
                    if (buf[i] == '$') {
                        state = WAIT_FOR_MSG;
                    } else {
                        buf[i]++;
                        if (send(sockfd, &buf[i], 1, 0) < 1) {
                            perror("ERROR send");
                            close(sockfd);
                            return;
                        }
                    }
                    break;
            }
        }
    }
    close(sockfd);
}