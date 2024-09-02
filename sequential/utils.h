#pragma once

#include <netinet/in.h>
#include <sys/socket.h>

// Logs error messages then quits application with failure code
void perror_die(const char* msg);

// Initializes and binds a socket to listen for connections at given port
int listen_socket(int portnum);

// Writes information about connected peer to stdout
void log_peer_connection(const struct sockaddr_in* sa, socklen_t sa_len);