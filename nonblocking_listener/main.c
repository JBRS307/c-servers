#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include "utils.h"

char* time_string(const time_t* timer) {
    char* str = ctime(timer);
    str[strlen(str) - 1] = '\0';
    return str;
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    int portnum = 3000;
    if (argc >= 2) {
        portnum = atoi(argv[1]);
    }
    printf("Listening on port %d\n", portnum);

    int sockfd = listen_socket(portnum);
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_length = sizeof(peer_addr);

    int newsockfd = accept(sockfd, (struct sockaddr*) &peer_addr, &peer_addr_length);
    if (newsockfd < 0) {
        perror_die("ERROR accept");
    }

    log_peer_connection(&peer_addr, peer_addr_length);

    int flags = fcntl(newsockfd, F_GETFL, 0);
    if (flags == -1) {
        perror_die("ERROR fcntl F_GETGL");
    }

    if (fcntl(newsockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror_die("ERROR fcntl F_SETFL");
    }

    while (1) {
        u_int8_t buf[1024];

        printf("Calling recv...\n");
        int len = recv(newsockfd, buf, sizeof(buf), 0);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000 * 500); // that is 500 millis
                continue;
            }
            perror_die("ERROR recv");
        } else if (len == 0) {
            printf("Peer disconnected!\n");
            break;
        }
        time_t curr_time;
        time(&curr_time);
        printf("%s: recv returned %d bytes\n", time_string(&curr_time), len);
    }

    close(newsockfd);
    close(sockfd);

    return EXIT_SUCCESS;
}