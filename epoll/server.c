#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include "utils.h"

#define MAXFDS 1024
#define SENDBUFSIZ 1024
#define RECVBUFSIZ 1024

typedef enum {
    INITIAL_ACK,
    WAIT_FOR_MSG,
    IN_MSG,
} ProcessingState;

typedef struct {
    ProcessingState state;
    uint8_t sendbuf[SENDBUFSIZ];
    int sendbuf_end;
    int sendptr;
} PeerState;

PeerState global_state[MAXFDS];

typedef struct {
    bool want_read;
    bool want_write;
} FDstatus;

const FDstatus FD_STATUS_R = {.want_read = true, .want_write = false};
const FDstatus FD_STATUS_W = {.want_read = false, .want_write = true};
const FDstatus FD_STATUS_RW = {.want_read = true, .want_write = true};
const FDstatus FD_STATUS_NORW = {.want_read = false, .want_write = false};

FDstatus on_peer_connected(int sockfd, struct sockaddr_in* peer_addr, socklen_t peer_addr_length);
FDstatus on_peer_ready_recv(int sockfd);
FDstatus on_peer_ready_send(int sockfd);

int main(int argc, char** argv) {
    int port = 3000;
    if (argc >= 2) {
        port = atoi(argv[1]);
    }
    if (port == 0) {
        die("Invalid port!\n");
    }
    printf("Listening on port %d\n", port);

    int listen_fd = listen_socket(port);
    set_nonblock_on_socket(listen_fd);

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror_die("ERROR epoll_create1");
    }

    struct epoll_event accept_event;
    accept_event.data.fd = listen_fd;
    accept_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &accept_event) < 0) {
        perror_die("ERROR epoll_ctl");
    }

    struct epoll_event* events = (struct epoll_event*)calloc(MAXFDS, sizeof(struct epoll_event));
    if (events == NULL) {
        die("Memory allocation error!\n");
    }

    while (1) {
        int nready = epoll_wait(epoll_fd, events, MAXFDS, -1);
        for (int i = 0; i < nready; i++) {
            if (events[i].events & EPOLLERR) {
                perror_die("epoll_wait returned EPOLLERR");
            }

            if (events[i].data.fd == listen_fd) {
                struct sockaddr_in peer_addr;
                socklen_t peer_addr_length = sizeof(peer_addr);
                int newsockfd = accept(listen_fd, (struct sockaddr*)&peer_addr, &peer_addr_length);
                if (newsockfd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        fprintf(stderr, "accept returned EAGAIN or EWOULDBLOCK\n");
                    } else {
                        perror_die("ERROR accept");
                    }
                } else {
                    set_nonblock_on_socket(newsockfd);
                    if (newsockfd >= MAXFDS) {
                        die("socket fd %d >= %d\n", newsockfd, MAXFDS);
                    }

                    FDstatus status = on_peer_connected(newsockfd, &peer_addr, peer_addr_length);
                    struct epoll_event event = {0};
                    event.data.fd = newsockfd;
                    if (status.want_read) {
                        event.events |= EPOLLIN;
                    }
                    if (status.want_write) {
                        event.events |= EPOLLOUT;
                    }

                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, newsockfd, &event) < 0) {
                        perror_die("ERROR epoll_ctl EPOLL_CTL_ADD");
                    }
                }
            } else {
                if (events[i].events & EPOLLIN) {
                    int fd = events[i].data.fd;
                    FDstatus status = on_peer_ready_recv(fd);
                    struct epoll_event event = {0};
                    event.data.fd = fd;

                    if (status.want_read) {
                        event.events |= EPOLLIN;
                    }
                    if (status.want_write) {
                        event.events |= EPOLLOUT;
                    }
                    if (event.events == 0) {
                        printf("Closing socket %d\n", fd);
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
                            perror_die("ERROR epoll_ctl EPOLL_CTL_DEL");
                        }
                        close(fd);
                    } else if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0) {
                        perror_die("ERROR epoll_ctl EPOLL_CTL_MOD");
                    }
                } else if (events[i].events & EPOLLOUT) {
                    int fd = events[i].data.fd;

                    FDstatus status = on_peer_ready_send(fd);
                    struct epoll_event event = {0};
                    event.data.fd = fd;

                    if (status.want_read) {
                        event.events |= EPOLLIN;
                    }
                    if (status.want_write) {
                        event.events |= EPOLLOUT;
                    }
                    if (event.events == 0) {
                        printf("Closing socket %d\n", fd);
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
                            perror_die("ERROR epoll_ctl EPOLL_CTL_DEL");
                        }
                        close(fd);
                    } else if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0) {
                        perror_die("ERROR epoll_ctl EPOLL_CTL_MOD");
                    }
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

FDstatus on_peer_connected(int sockfd, struct sockaddr_in* peer_addr, socklen_t peer_addr_length) {
    assert(sockfd < MAXFDS);
    log_peer_connection(peer_addr, peer_addr_length);

    PeerState* peer_state = &global_state[sockfd];
    peer_state->state = INITIAL_ACK;
    peer_state->sendbuf[0] = '*';
    peer_state->sendptr = 0;
    peer_state->sendbuf_end = 1;

    return FD_STATUS_W;
}

FDstatus on_peer_ready_recv(int sockfd) {
    assert(sockfd < MAXFDS);
    PeerState* peer_state = &global_state[sockfd];

    if (peer_state->state == INITIAL_ACK ||
        peer_state->sendptr < peer_state->sendbuf_end) {
        return FD_STATUS_W;
    }

    uint8_t buf[RECVBUFSIZ];
    int nbytes = recv(sockfd, (void*)buf, RECVBUFSIZ, 0);
    if (nbytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "recv returned EAGAINA or EWOULDBLOCK");
            return FD_STATUS_R;
        }
        perror_die("ERROR recv");
    } else if (nbytes == 0) {
        return FD_STATUS_NORW;
    }

    bool ready_to_send = false;
    for (int i = 0; i < nbytes; i++) {
        switch (peer_state->state) {
            case WAIT_FOR_MSG:
                if (buf[i] == '^') {
                    peer_state->state = IN_MSG;
                }
                break;
            case IN_MSG:
                if (buf[i] == '$') {
                    peer_state->state = WAIT_FOR_MSG;
                } else {
                    assert(peer_state->sendbuf_end < SENDBUFSIZ);
                    peer_state->sendbuf[peer_state->sendbuf_end++] = buf[i] + 1;
                    ready_to_send = true;
                }
                break;
            case INITIAL_ACK:
                assert(0 && "Unreachable");
                break;
        }
    }
    return (FDstatus){.want_read = !ready_to_send, .want_write = ready_to_send};
}

FDstatus on_peer_ready_send(int sockfd) {
    assert(sockfd < MAXFDS);

    PeerState* peer_state = &global_state[sockfd];

    if (peer_state->sendptr >= peer_state->sendbuf_end) {
        return FD_STATUS_RW;
    }

    int sendlen = peer_state->sendbuf_end - peer_state->sendptr;
    int nsent = send(sockfd, (void*)&peer_state->sendbuf[peer_state->sendptr], (size_t)sendlen, 0);
    if (nsent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "Socket %d returned EAGAIN or EWOULDBLOCK\n", sockfd);
            return FD_STATUS_W;
        }
        perror_die("ERROR send");
    }

    if (nsent < sendlen) {
        peer_state->sendptr += nsent;
        return FD_STATUS_W;
    } else {
        peer_state->sendptr = 0;
        peer_state->sendbuf_end = 0;

        if (peer_state->state == INITIAL_ACK) {
            peer_state->state = WAIT_FOR_MSG;
        }

        return FD_STATUS_R;
    }
}