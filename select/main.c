#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>

#include "utils.h"

#define SENDBUF_SIZE 1024

typedef enum {
    INITIAL_ACK,
    WAIT_FOR_MSG,
    IN_MSG
} ProcessingState;

typedef struct {
    ProcessingState state;
    uint8_t sendbuf[SENDBUF_SIZE];
    int sendbuf_end;
    int sendptr;
} peer_state_t;

typedef struct {
    bool want_read;
    bool want_write;
} fd_status_t;

const fd_status_t fd_status_R = {.want_read = true, .want_write = false};
const fd_status_t fd_status_W = {.want_read = false, .want_write = true};
const fd_status_t fd_status_RW = {.want_read = true, .want_write = true};
const fd_status_t fd_status_NORW = {.want_read = false, .want_write = false};


fd_status_t on_peer_connected(int sockfd, const struct sockaddr_in* peer_addr, socklen_t peer_addr_length);
fd_status_t on_peer_ready_recv(int sockfd);
fd_status_t on_peer_ready_send(int sockfd);

peer_state_t global_state[FD_SETSIZE];

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    int portnum = 3000;
    if (argc >= 2) {
        portnum = atoi(argv[1]);
    }
    printf("Listening on port %d\n", portnum);

    int listener_sockfd = listen_socket(portnum);
    set_nonblock_on_socket(listener_sockfd);

    fd_set readfds_master;
    fd_set writefds_master;
    FD_ZERO(&readfds_master);
    FD_ZERO(&writefds_master);

    FD_SET(listener_sockfd, &readfds_master);

    // MAX FD (nfds) in set tracker
    int fdset_max = listener_sockfd;

    while (1) {
        fd_set readfds = readfds_master;
        fd_set writefds = writefds_master;

        int nready = select(fdset_max + 1, &readfds, &writefds, NULL, NULL);
        if (nready < 0) {
            perror_die("ERROR select");
        }

        for (int fd = 0; fd <= fdset_max && nready > 0; fd++) {
            if (FD_ISSET(fd, &readfds)) {
                nready--;

                if (fd == listener_sockfd) {
                    struct sockaddr_in peer_addr;
                    socklen_t peer_addr_length = sizeof(peer_addr);

                    int newsockfd = accept(listener_sockfd, (struct sockaddr*) &peer_addr, &peer_addr_length);
                    if (newsockfd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            printf("accept() returned EAGAIN or EWOULDBLOCK\n");
                        } else {
                            perror_die("ERROR accept");
                        }
                    } else {
                        set_nonblock_on_socket(newsockfd);
                        if (newsockfd > fdset_max) {
                            if (newsockfd >= FD_SETSIZE) {
                                die("sockfd (%d) >= FD_SETSIZE (%d)", newsockfd, FD_SETSIZE);
                            }
                            fdset_max = newsockfd;
                        }    
                        fd_status_t status = on_peer_connected(newsockfd, &peer_addr, peer_addr_length);

                        if (status.want_read) {
                            FD_SET(newsockfd, &readfds_master);
                        } else {
                            FD_CLR(newsockfd, &readfds_master);
                        }
                        if (status.want_write) {
                            FD_SET(newsockfd, &writefds_master);
                        } else {
                            FD_CLR(newsockfd, &writefds_master);
                        }
                    }
                } else {
                    fd_status_t status = on_peer_ready_recv(fd);

                    if (status.want_read) {
                        FD_SET(fd, &readfds_master);
                    } else {
                        FD_CLR(fd, &readfds_master);
                    }
                    if (status.want_write) {
                        FD_SET(fd, &writefds_master);
                    } else {
                        FD_CLR(fd, &writefds_master);
                    }
                    if (!status.want_read && !status.want_write) {
                        printf("Closing socket %d\n", fd);
                        close(fd);
                    }
                }
            }

            if (FD_ISSET(fd, &writefds)) {
                nready--;

                fd_status_t status = on_peer_ready_send(fd);

                if (status.want_read) {
                    FD_SET(fd, &readfds_master);
                } else {
                    FD_CLR(fd, &readfds_master);
                }
                if (status.want_write) {
                    FD_SET(fd, &writefds_master);
                } else {
                    FD_CLR(fd, &writefds_master);
                }
                if (!status.want_read && !status.want_write) {
                    printf("Closing socket %d\n", fd);
                    close(fd);
                }
            }

        }
    }

    return EXIT_SUCCESS;
}

fd_status_t on_peer_connected(int sockfd, const struct sockaddr_in* peer_addr, socklen_t peer_addr_length) {
    assert(sockfd < FD_SETSIZE);

    log_peer_connection(peer_addr, peer_addr_length);

    peer_state_t* peerstate = &global_state[sockfd];

    peerstate->state = INITIAL_ACK;
    peerstate->sendbuf[0] = '*';
    peerstate->sendptr = 0;
    peerstate->sendbuf_end = 1;

    return fd_status_W; 
}

fd_status_t on_peer_ready_recv(int sockfd) {
    assert(sockfd < FD_SETSIZE);

    peer_state_t* peerstate = &global_state[sockfd];

    if (peerstate->state == INITIAL_ACK ||
        peerstate->sendptr < peerstate->sendbuf_end) {
        return fd_status_W;
    }

    uint8_t buf[1024];

    int nbytes = recv(sockfd, buf, sizeof(buf), 0);
    if (nbytes == 0) {
        return fd_status_NORW;
    } else if (nbytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return fd_status_R;
        }
        perror_die("ERROR recv");
    }

    bool ready_to_send = false;
    for (int i = 0; i < nbytes; i++) {
        switch (peerstate->state) {
            case INITIAL_ACK:
                assert(0 && "unreachable");
                break;
            case WAIT_FOR_MSG:
                if (buf[i] == '^') {
                    peerstate->state = IN_MSG;
                }
                break;
            case IN_MSG:
                if (buf[i] == '$') {
                    peerstate->state = WAIT_FOR_MSG;
                } else {
                    assert(peerstate->sendbuf_end < SENDBUF_SIZE);
                    peerstate->sendbuf[peerstate->sendbuf_end++] = buf[i] + 1;
                    ready_to_send = true;
                }
                break;
        }
    }

    return (fd_status_t){.want_read = !ready_to_send, .want_write = ready_to_send};    
}

fd_status_t on_peer_ready_send(int sockfd) {
    assert(sockfd < FD_SETSIZE);

    peer_state_t* peerstate = &global_state[sockfd];

    if (peerstate->sendptr >= peerstate->sendbuf_end) {
        return fd_status_RW;
    }

    int sendlen = peerstate->sendbuf_end - peerstate->sendptr;
    int nsent = send(sockfd, &peerstate->sendbuf[peerstate->sendptr], sendlen, 0);
    if (nsent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return fd_status_W;
        }
        perror_die("ERROR send");
    }

    if (nsent < sendlen) {
        peerstate->sendptr += nsent;
        return fd_status_W;
    } else {
        peerstate->sendptr = 0;
        peerstate->sendbuf_end = 0;

        if (peerstate->state == INITIAL_ACK) {
            peerstate->state = WAIT_FOR_MSG;
        }
        return fd_status_R;
    }
}