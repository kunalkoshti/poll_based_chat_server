#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT "3490"

using namespace std;

// Global state for managing the array of file descriptors for poll()
int fd_size = 5;  // Initial capacity of our pollfd array
int fd_count = 0; // Current number of active descriptors in the array
int listening_fd; // The main socket that listens for new connections
struct pollfd *pfds = (struct pollfd *)malloc(sizeof(struct pollfd) * 5);

/**
 * Helper to convert a sockaddr (IPv4 or IPv6) into a readable string.
 */
const char *inet_ntop2(sockaddr_storage *addr, char *buf, size_t size)
{
    struct sockaddr_storage *sas = addr;
    struct sockaddr_in *sa4;
    struct sockaddr_in6 *sa6;
    void *src;

    switch (sas->ss_family)
    {
    case AF_INET:
        sa4 = (struct sockaddr_in *)addr;
        src = &(sa4->sin_addr);
        break;
    case AF_INET6:
        sa6 = (struct sockaddr_in6 *)addr;
        src = &(sa6->sin6_addr);
        break;
    default:
        return NULL;
    }

    return inet_ntop(sas->ss_family, src, buf, size);
}

/**
 * Sets up a socket to listen for incoming connections.
 * Uses getaddrinfo for protocol-agnostic setup.
 */
int get_listener_socket()
{
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // Hardcoded to IPv4 for this version
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // Fill in my IP for me

    if (getaddrinfo(NULL, PORT, &hints, &res) != 0)
        return -1;

    int listen_fd = -1;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next)
    {
        listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listen_fd == -1)
            continue;

        // Allow immediate reuse of the port after server restart
        int yes = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listen_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(listen_fd);
            continue;
        }
        break; // Successfully bound
    }
    freeaddrinfo(res);
    return listen_fd;
}

/**
 * Adds a new file descriptor to the pollfd array.
 * Resizes the array (doubles it) if it runs out of space.
 */
void add_to_pfds(int newfd)
{
    if (fd_count == fd_size)
    {
        fd_size *= 2;
        pfds = (pollfd *)realloc(pfds, sizeof(pollfd) * fd_size);
    }
    pfds[fd_count].fd = newfd;
    pfds[fd_count].events = POLLIN; // Only interested in "ready to read" events
    pfds[fd_count].revents = 0;
    fd_count++;
}

/**
 * Removes a file descriptor from the array by swapping it with the last element.
 */
void delete_from_pfds(int *i)
{
    pfds[*i] = pfds[fd_count - 1];
    fd_count--;
}

/**
 * Reads data from a client and broadcasts it to all other connected clients.
 */
void handle_client_data(int *i)
{
    char buf[256];
    int nbytes = recv(pfds[*i].fd, buf, sizeof buf, 0);
    int sender_fd = pfds[*i].fd;

    if (nbytes <= 0) // Error or connection closed
    {
        if (nbytes == 0)
            printf("pollserver: socket %d hung up\n", sender_fd);
        else
            cout << "recv error" << endl;

        close(sender_fd);
        delete_from_pfds(i);
        (*i)--; // Decrement i so the loop doesn't skip the swapped element
    }
    else
    {
        // Data received: broadcast it!
        printf("pollserver: recv from fd %d: %.*s", sender_fd, nbytes, buf);
        for (int j = 0; j < fd_count; j++)
        {
            int dest_fd = pfds[j].fd;
            // Send to everyone except the listener and the sender
            if (dest_fd != listening_fd && dest_fd != sender_fd)
            {
                if (send(dest_fd, buf, nbytes, 0) == -1)
                    perror("send");
            }
        }
    }
}

/**
 * Accepts a new incoming connection and adds it to our monitoring list.
 */
void handle_new_connections()
{
    struct sockaddr_storage clientaddr;
    socklen_t addrlen = sizeof clientaddr;
    char remoteIP[INET6_ADDRSTRLEN];

    int newfd = accept(listening_fd, (struct sockaddr *)&clientaddr, &addrlen);
    if (newfd == -1)
    {
        perror("accept");
        return;
    }

    add_to_pfds(newfd);
    printf("pollserver: new connection from %s on socket %d\n",
           inet_ntop2(&clientaddr, remoteIP, sizeof remoteIP),
           newfd);
}

/**
 * Iterates through the pollfd array to see which sockets have events.
 */
void process_connections()
{
    for (int i = 0; i < fd_count; i++)
    {
        // Check if data is incoming or if the connection was hung up
        if (pfds[i].revents & (POLLIN | POLLHUP))
        {
            if (pfds[i].fd == listening_fd)
            {
                handle_new_connections(); // New user trying to join
            }
            else
            {
                handle_client_data(&i); // Existing user sent a message
            }
        }
    }
}

int main()
{
    listening_fd = get_listener_socket();
    if (listen(listening_fd, 10) == -1)
    {
        perror("listen");
        exit(1);
    }

    // Add the listener socket as the first element in the array
    pfds[0].fd = listening_fd;
    pfds[0].events = POLLIN;
    fd_count = 1;

    printf("Server started on port %s. Waiting for connections...\n", PORT);

    while (true)
    {
        // poll() blocks here until an event occurs on one of the fds
        int num_events = poll(pfds, fd_count, -1);
        if (num_events == -1)
        {
            perror("poll");
            exit(1);
        }

        process_connections();
    }

    free(pfds);
    return 0;
}