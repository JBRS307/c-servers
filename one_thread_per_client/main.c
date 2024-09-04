#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "utils.h"

typedef struct {
    int sockfd;
} thread_config_t;

typedef enum {
    WAITING_FOR_MSG,
    IN_MSG,
} ProcessingState;

void* server_thread(void* arg);
void serve_connection(int sockfd);

int main(int argc, char** argv) {
    int portnum = 3000;
    if (argc >= 2) {
        portnum = atoi(argv[1]);
    }

    int sockfd = listen_socket(portnum);
    printf("Listening on port %d\n", portnum);

    while (1) {
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_length = sizeof(peer_addr);

        int newsockfd = accept(sockfd, (struct sockaddr*) &peer_addr, &peer_addr_length);
        if (newsockfd < 0) {
            perror_die("ERROR accept");
        }

        log_peer_connection(&peer_addr, peer_addr_length);

        pthread_t thread;
        thread_config_t* config = (thread_config_t*)malloc(sizeof(thread_config_t));
        if (config == NULL) {
            die("Memory allocation error with malloc!");
        }

        config->sockfd = newsockfd;
        pthread_create(&thread, NULL, server_thread, config);


    }

    return EXIT_SUCCESS;
}

void* server_thread(void* arg) {
    thread_config_t* config = (thread_config_t*)arg;

    int sockfd = config->sockfd;
    free(config);

    unsigned long id = (unsigned long)pthread_self();
    printf("Thread %lu created to handle connection with socket %d\n", id, sockfd);
    serve_connection(sockfd);
    printf("Thread %lu done!\n", id);
    return NULL;
}

void serve_connection(int sockfd) {
    if (send(sockfd, "*", 1, 0) < 1) {
        perror_die("ERROR send");
    }

    ProcessingState state = WAITING_FOR_MSG;

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
                case WAITING_FOR_MSG:
                    if (buf[i] == '^') {
                        state = IN_MSG;
                    }
                    break;
                case IN_MSG:
                    if (buf[i] == '$') {
                        state = WAITING_FOR_MSG;
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