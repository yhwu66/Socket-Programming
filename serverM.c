/*
** pollserver.c -- a cheezy multiperson chat server
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>

#define TCP_PORT_CLIENTA "25483" // Port we're listening on
#define TCP_PORT_CLIENTB "26483"
#define UDP_PORT "24483"
#define PORT_SERVERA "21483"
#define PORT_SERVERB "22483"
#define PORT_SERVERC "23483"
#define LOCALHOST "127.0.0.1"
#define MAXDATASIZE 512

// Get sockaddr, IPv4 or IPv6: //
//codes from Beej's
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// Return a listening socket
//codes edited from Beej's
int get_listener_socket(char clientLabel)
{
    int listener; // Listening socket descriptor
    int yes = 1;  // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    char *curPort;
    if (clientLabel == 'A')
    {
        curPort = TCP_PORT_CLIENTA;
    }
    else if (clientLabel == 'B')
    {
        curPort = TCP_PORT_CLIENTB;
    }
    if ((rv = getaddrinfo(LOCALHOST, curPort, &hints, &ai)) != 0)
    {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next)
    {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0)
        {
            continue;
        }

        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(listener);
            continue;
        }

        break;
    }

    // If we got here, it means we didn't get bound
    if (p == NULL)
    {
        return -1;
    }

    freeaddrinfo(ai); // All done with this

    // Listen
    if (listen(listener, 10) == -1)
    {
        return -1;
    }

    return listener;
}

// Add a new file descriptor to the set
//codes from Beej's
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size)
    {
        *fd_size *= 2; // Double it

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

// Remove an index from the set
//codes from Beej's
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count - 1];

    (*fd_count)--;
}

// Main
int main()
{
    int operation = 0; //1 for checking ballance; 2 for transaction
    int listener_cA;   // Listening socket descriptor
    int listener_cB;   // Listening socket descriptor
    int numbytes;
    int newfd;                          // Newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;

    char buf[MAXDATASIZE]; // Buffer for client data

    char remoteIP[INET6_ADDRSTRLEN];

    // Start off with room for 5 connections
    // (We'll realloc as necessary)
    int fd_count = 0;
    int fd_size = 5;
    struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

    //** Set up and get 2 listening socket(TCP)
    //codes from Beej's
    listener_cA = get_listener_socket('A');
    listener_cB = get_listener_socket('B');

    if (listener_cA == -1)
    {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }
    if (listener_cB == -1)
    {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    // Add the listener_cA to set
    pfds[0].fd = listener_cA;
    pfds[0].events = POLLIN; // Report ready to read on incoming connection
    // Add the listener_cB to set
    pfds[1].fd = listener_cB;
    pfds[1].events = POLLIN; // Report ready to read on incoming connection

    fd_count = 2; // For the listener_cA and the listener_cB
    //** End of setting up of TCP ports for clientA and clientB

    // Main loop
    //edited from Beej's codes
    printf("The main server is up and running\n");
    for (;;)
    {

        //printf("position 1 ok\n");

        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count == -1)
        {
            perror("poll");
            exit(1);
        }

        // Run through the existing connections looking for data to read
        for (int i = 0; i < fd_count; i++)
        {

            // Check if someone's ready to read
            if (pfds[i].revents & POLLIN)
            { // We got one!!

                if (pfds[i].fd == listener_cA)
                {
                    // If listener_cA is ready to read, handle new connection

                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener_cA,
                                   (struct sockaddr *)&remoteaddr,
                                   &addrlen);

                    if (newfd == -1)
                    {
                        perror("accept");
                    }
                    else
                    {
                        add_to_pfds(&pfds, newfd, &fd_count, &fd_size);

                        if ((numbytes = recv(newfd, &operation, sizeof(int), 0)) == -1)
                        {
                            perror("recv");
                            exit(1);
                        }
                        //printf("%d\n",operation);
                        if (operation == 1)
                        {
                            //** Set up UDP soket
                            //codes from Beej's
                            int sockfd_udp;
                            struct addrinfo hints_udp, *servinfo_udp, *p_udp;
                            int rv_udp;
                            int numbytes_udp;
                            struct sockaddr_storage their_addr;
                            socklen_t addr_len;

                            memset(&hints_udp, 0, sizeof hints_udp);
                            hints_udp.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_udp.ai_socktype = SOCK_DGRAM;

                            if ((rv_udp = getaddrinfo("localhost", UDP_PORT, &hints_udp, &servinfo_udp)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo udp: %s\n", gai_strerror(rv_udp));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_udp = servinfo_udp; p_udp != NULL; p_udp = p_udp->ai_next)
                            {
                                if ((sockfd_udp = socket(p_udp->ai_family, p_udp->ai_socktype,
                                                         p_udp->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_udp == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_udp);
                            //** End of setting up UDP socket
                            //** Set up serverA soket
                            //codes from Beej's
                            int sockfd_serverA;
                            struct addrinfo hints_serverA, *servinfo_serverA, *p_serverA;
                            int rv_serverA;
                            //int numbytes_serverA;

                            memset(&hints_serverA, 0, sizeof hints_serverA);
                            hints_serverA.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverA.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverA = getaddrinfo("localhost", PORT_SERVERA, &hints_serverA, &servinfo_serverA)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverA));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverA = servinfo_serverA; p_serverA != NULL; p_serverA = p_serverA->ai_next)
                            {
                                if ((sockfd_serverA = socket(p_serverA->ai_family, p_serverA->ai_socktype,
                                                             p_serverA->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverA == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverA);
                            //** End of setting up serverA socket

                            //printf("%d\n",operation);
                            if ((numbytes = recv(newfd, buf, MAXDATASIZE - 1, 0)) == -1)
                            {
                                perror("recv");
                                exit(1);
                            }
                            buf[numbytes] = '\0';
                            printf("The main server received input =\"%s\" from the client using TCP over port %s\n", buf, TCP_PORT_CLIENTA);

                            //***sending A
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, buf, MAXDATASIZE - 1, 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server A\n");
                            addr_len = sizeof their_addr;
                            int balance_changeA = 0;
                            bool isInNetworkA = false;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkA, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeA, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            printf("The main server received transactions from Server A using UDP over port %s\n", PORT_SERVERA);
                            //end sending A

                            //** Set up serverB soket
                            //codes from Beej's
                            int sockfd_serverB;
                            struct addrinfo hints_serverB, *servinfo_serverB, *p_serverB;
                            int rv_serverB;
                            //int numbytes_serverA;

                            memset(&hints_serverB, 0, sizeof hints_serverB);
                            hints_serverB.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverB.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverB = getaddrinfo("localhost", PORT_SERVERB, &hints_serverB, &servinfo_serverB)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverB));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverB = servinfo_serverB; p_serverB != NULL; p_serverB = p_serverB->ai_next)
                            {
                                if ((sockfd_serverB = socket(p_serverB->ai_family, p_serverB->ai_socktype,
                                                             p_serverB->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverB == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverB);
                            //** End of setting up serverB socket
                            //***sending B
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, buf, MAXDATASIZE - 1, 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server B\n");
                            addr_len = sizeof their_addr;
                            int balance_changeB = 0;
                            bool isInNetworkB = false;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkB, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeB, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            printf("The main server received transactions from Server B using UDP over port %s\n", PORT_SERVERB);
                            //end sending B
                            //** Set up serverC soket
                            //codes from Beej's
                            int sockfd_serverC;
                            struct addrinfo hints_serverC, *servinfo_serverC, *p_serverC;
                            int rv_serverC;

                            memset(&hints_serverC, 0, sizeof hints_serverC);
                            hints_serverC.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverC.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverC = getaddrinfo("localhost", PORT_SERVERC, &hints_serverC, &servinfo_serverC)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverC));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverC = servinfo_serverC; p_serverC != NULL; p_serverC = p_serverC->ai_next)
                            {
                                if ((sockfd_serverC = socket(p_serverC->ai_family, p_serverC->ai_socktype,
                                                             p_serverC->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverC == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverC);
                            //** End of setting up serverC socket
                            //***sending C
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, buf, MAXDATASIZE - 1, 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server C\n");
                            addr_len = sizeof their_addr;
                            int balance_changeC = 0;
                            bool isInNetworkC = false;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkC, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeC, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            printf("The main server received transactions from Server C using UDP over port %s\n", PORT_SERVERC);
                            //end sending C

                            int balance = -1;
                            if (isInNetworkA || isInNetworkB || isInNetworkC)
                            {
                                balance = 1000 + balance_changeA + balance_changeB + balance_changeC;
                            }
                            if (send(newfd, &balance, sizeof(int), 0) == -1)
                                perror("send");
                            printf("The main server sent the current balance to client A\n");
                        }

                        else if (operation == 2)
                        {
                            //** Set up UDP soket
                            //codes from Beej's
                            int sockfd_udp;
                            struct addrinfo hints_udp, *servinfo_udp, *p_udp;
                            int rv_udp;
                            int numbytes_udp;
                            struct sockaddr_storage their_addr;
                            socklen_t addr_len;

                            memset(&hints_udp, 0, sizeof hints_udp);
                            hints_udp.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_udp.ai_socktype = SOCK_DGRAM;

                            if ((rv_udp = getaddrinfo("localhost", UDP_PORT, &hints_udp, &servinfo_udp)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo udp: %s\n", gai_strerror(rv_udp));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_udp = servinfo_udp; p_udp != NULL; p_udp = p_udp->ai_next)
                            {
                                if ((sockfd_udp = socket(p_udp->ai_family, p_udp->ai_socktype,
                                                         p_udp->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_udp == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_udp);
                            //** End of setting up UDP socket
                            //** Set up serverA soket
                            //codes from Beej's
                            int sockfd_serverA;
                            struct addrinfo hints_serverA, *servinfo_serverA, *p_serverA;
                            int rv_serverA;
                            //int numbytes_serverA;

                            memset(&hints_serverA, 0, sizeof hints_serverA);
                            hints_serverA.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverA.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverA = getaddrinfo("localhost", PORT_SERVERA, &hints_serverA, &servinfo_serverA)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverA));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverA = servinfo_serverA; p_serverA != NULL; p_serverA = p_serverA->ai_next)
                            {
                                if ((sockfd_serverA = socket(p_serverA->ai_family, p_serverA->ai_socktype,
                                                             p_serverA->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverA == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverA);
                            //** End of setting up serverA socket

                            //codes from Beej's
                            //printf("%d\n",operation);
                            char sender[MAXDATASIZE];
                            char receiver[MAXDATASIZE];
                            char amountstr[MAXDATASIZE];
                            //printf("%d\n",operation);
                            if ((numbytes = recv(newfd, sender, MAXDATASIZE - 1, 0)) == -1)
                            {
                                perror("recv");
                                exit(1);
                            }
                            sender[numbytes] = '\0';

                            if ((numbytes = recv(newfd, receiver, MAXDATASIZE - 1, 0)) == -1)
                            {
                                perror("recv");
                                exit(1);
                            }
                            receiver[numbytes] = '\0';

                            if ((numbytes = recv(newfd, amountstr, MAXDATASIZE - 1, 0)) == -1)
                            {
                                perror("recv");
                                exit(1);
                            }
                            amountstr[numbytes] = '\0';

                            printf("The main server received from %s to transfer %s coins to %s using TCP over port %s\n", sender, amountstr, receiver, TCP_PORT_CLIENTA);

                            //** sending A
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, sender, MAXDATASIZE - 1, 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, receiver, MAXDATASIZE - 1, 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server A\n");

                            addr_len = sizeof their_addr;
                            int balance_changeA = 0;
                            bool isInNetworkA = false;
                            bool isInNetworkA_rec = false;
                            int maxSeqA = 0;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkA, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkA_rec, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeA, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &maxSeqA, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            printf("The main server received the feedback from Server A using UDP over port %s\n", PORT_SERVERA);
                            //**end sending A
                            //** Set up serverB soket
                            //codes from Beej's
                            int sockfd_serverB;
                            struct addrinfo hints_serverB, *servinfo_serverB, *p_serverB;
                            int rv_serverB;
                            //int numbytes_serverA;

                            memset(&hints_serverB, 0, sizeof hints_serverB);
                            hints_serverB.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverB.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverB = getaddrinfo("localhost", PORT_SERVERB, &hints_serverB, &servinfo_serverB)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverB));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverB = servinfo_serverB; p_serverB != NULL; p_serverB = p_serverB->ai_next)
                            {
                                if ((sockfd_serverB = socket(p_serverB->ai_family, p_serverB->ai_socktype,
                                                             p_serverB->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverB == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverB);
                            //** End of setting up serverB socket
                            //***sending B
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, sender, MAXDATASIZE - 1, 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, receiver, MAXDATASIZE - 1, 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server B\n");
                            addr_len = sizeof their_addr;
                            int balance_changeB = 0;
                            bool isInNetworkB = false;
                            bool isInNetworkB_rec = false;
                            int maxSeqB = 0;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkB, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkB_rec, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeB, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &maxSeqB, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            printf("The main server received the feedback from Server B using UDP over port %s\n", PORT_SERVERB);
                            //end sending B
                            //** Set up serverC soket
                            //codes from Beej's
                            int sockfd_serverC;
                            struct addrinfo hints_serverC, *servinfo_serverC, *p_serverC;
                            int rv_serverC;

                            memset(&hints_serverC, 0, sizeof hints_serverC);
                            hints_serverC.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverC.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverC = getaddrinfo("localhost", PORT_SERVERC, &hints_serverC, &servinfo_serverC)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverC));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverC = servinfo_serverC; p_serverC != NULL; p_serverC = p_serverC->ai_next)
                            {
                                if ((sockfd_serverC = socket(p_serverC->ai_family, p_serverC->ai_socktype,
                                                             p_serverC->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverC == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverC);
                            //** End of setting up serverC socket
                            //***sending C
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, sender, MAXDATASIZE - 1, 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, receiver, MAXDATASIZE - 1, 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server C\n");
                            addr_len = sizeof their_addr;
                            int balance_changeC = 0;
                            bool isInNetworkC = false;
                            bool isInNetworkC_rec = false;
                            int maxSeqC = 0;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkC, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkC_rec, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeC, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &maxSeqC, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            printf("The main server received the feedback from Server C using UDP over port %s\n", PORT_SERVERC);
                            //end sending C

                            //printf("maxSeqA: %d\n",maxSeqA);
                            //printf("maxSeqB: %d\n",maxSeqB);
                            //printf("maxSeqC: %d\n",maxSeqC);
                            int curSeq = maxSeqA;
                            if (maxSeqB > curSeq)
                            {
                                curSeq = maxSeqB;
                            }
                            if (maxSeqC > curSeq)
                            {
                                curSeq = maxSeqC;
                            }
                            //printf("curSeq: %d\n", curSeq);
                            int balance2 = -1;
                            bool isSenderInNetwork = false;
                            bool isReceiverInNetwork = false;
                            int transamount = atoi(amountstr);
                            //printf("%d\n", transamount);
                            if (isInNetworkA || isInNetworkB || isInNetworkC)
                            {
                                balance2 = 1000 + balance_changeA + balance_changeB + balance_changeC;
                                isSenderInNetwork = true;
                            }
                            if (isInNetworkA_rec || isInNetworkB_rec || isInNetworkC_rec)
                            {
                                isReceiverInNetwork = true;
                            }

                            int status = 0; //1 for seccess,2 for insufficent balance2, 3 for sender out network,4 for receiver out network, 5 for 2 out network
                            if (isSenderInNetwork && isReceiverInNetwork)
                            {
                                if (balance2 >= transamount)
                                {
                                    status = 1;
                                    balance2 -= transamount;
                                }
                                else
                                {
                                    status = 2;
                                }
                            }
                            else if (isSenderInNetwork)
                            {
                                status = 4;
                            }
                            else if (isReceiverInNetwork)
                            {
                                status = 3;
                            }
                            else
                            {
                                status = 5;
                            }
                            //***write to logfile
                            if (status == 1)
                            {
                                int r = rand() % 3;
                                //printf("random number:%d\n", r);
                                int numrandom = r + 1;
                                char *currentPort;
                                if (numrandom == 1)
                                {
                                    currentPort = PORT_SERVERA;
                                }
                                else if (numrandom == 2)
                                {
                                    currentPort = PORT_SERVERB;
                                }
                                else
                                {
                                    currentPort = PORT_SERVERC;
                                }
                                //** Set up wt soket
                                //codes from Beej's
                                int sockfd_wt;
                                struct addrinfo hints_wt, *servinfo_wt, *p_wt;
                                int rv_wt;

                                memset(&hints_wt, 0, sizeof hints_wt);
                                hints_wt.ai_family = AF_INET6; // set to AF_INET to use IPv4
                                hints_wt.ai_socktype = SOCK_DGRAM;

                                if ((rv_wt = getaddrinfo("localhost", currentPort, &hints_wt, &servinfo_wt)) != 0)
                                {
                                    fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_wt));
                                    return 1;
                                }

                                // loop through all the results and make a socket
                                for (p_wt = servinfo_wt; p_wt != NULL; p_wt = p_wt->ai_next)
                                {
                                    if ((sockfd_wt = socket(p_wt->ai_family, p_wt->ai_socktype,
                                                            p_wt->ai_protocol)) == -1)
                                    {
                                        perror("talker: socket");
                                        continue;
                                    }

                                    break;
                                }

                                if (p_wt == NULL)
                                {
                                    fprintf(stderr, "talker: failed to create socket\n");
                                    return 2;
                                }
                                freeaddrinfo(servinfo_wt);
                                //** End of setting up wt socket
                                int tmp_operation = 3;
                                if ((numbytes_udp = sendto(sockfd_udp, &tmp_operation, sizeof(int), 0,
                                                           p_wt->ai_addr, p_wt->ai_addrlen)) == -1)
                                {
                                    perror("talker: sendto 1");
                                    exit(1);
                                }

                                int newSeq = curSeq + 1;
                                if ((numbytes_udp = sendto(sockfd_udp, &newSeq, sizeof(int), 0,
                                                           p_wt->ai_addr, p_wt->ai_addrlen)) == -1)
                                {
                                    perror("talker: sendto 1");
                                    exit(1);
                                }
                                if ((numbytes_udp = sendto(sockfd_udp, sender, MAXDATASIZE - 1, 0,
                                                           p_wt->ai_addr, p_wt->ai_addrlen)) == -1)
                                {
                                    perror("talker: sendto 1");
                                    exit(1);
                                }
                                if ((numbytes_udp = sendto(sockfd_udp, receiver, MAXDATASIZE - 1, 0,
                                                           p_wt->ai_addr, p_wt->ai_addrlen)) == -1)
                                {
                                    perror("talker: sendto 1");
                                    exit(1);
                                }
                                if ((numbytes_udp = sendto(sockfd_udp, &transamount, sizeof(int), 0,
                                                           p_wt->ai_addr, p_wt->ai_addrlen)) == -1)
                                {
                                    perror("talker: sendto 1");
                                    exit(1);
                                }
                            }
                            //***end of writing to logfile

                            if (send(newfd, &status, sizeof(int), 0) == -1)
                                perror("send");

                            if (send(newfd, &balance2, sizeof(int), 0) == -1)
                                perror("send");
                            printf("The main server sent the result of transaction to client A\n");
                        }

                        else if (operation == 4)
                        {
                            printf("A TXLIST request has been received\n");
                            //** Set up UDP soket
                            //codes from Beej's
                            int sockfd_udp;
                            struct addrinfo hints_udp, *servinfo_udp, *p_udp;
                            int rv_udp;
                            int numbytes_udp;
                            struct sockaddr_storage their_addr;
                            socklen_t addr_len;

                            memset(&hints_udp, 0, sizeof hints_udp);
                            hints_udp.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_udp.ai_socktype = SOCK_DGRAM;

                            if ((rv_udp = getaddrinfo("localhost", UDP_PORT, &hints_udp, &servinfo_udp)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo udp: %s\n", gai_strerror(rv_udp));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_udp = servinfo_udp; p_udp != NULL; p_udp = p_udp->ai_next)
                            {
                                if ((sockfd_udp = socket(p_udp->ai_family, p_udp->ai_socktype,
                                                         p_udp->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_udp == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_udp);
                            //** End of setting up UDP socket
                            //** Set up serverA soket
                            //codes from Beej's
                            int sockfd_serverA;
                            struct addrinfo hints_serverA, *servinfo_serverA, *p_serverA;
                            int rv_serverA;
                            //int numbytes_serverA;

                            memset(&hints_serverA, 0, sizeof hints_serverA);
                            hints_serverA.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverA.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverA = getaddrinfo("localhost", PORT_SERVERA, &hints_serverA, &servinfo_serverA)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverA));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverA = servinfo_serverA; p_serverA != NULL; p_serverA = p_serverA->ai_next)
                            {
                                if ((sockfd_serverA = socket(p_serverA->ai_family, p_serverA->ai_socktype,
                                                             p_serverA->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverA == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverA);
                            //** End of setting up serverA socket

                            FILE *fptrA;
                            fptrA = (fopen("tmp.txt", "w"));
                            if (fptrA == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending A
                            int number_of_linesA = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesA, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufA[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesA; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufA, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufA);
                                fprintf(fptrA, "%s", line_bufA);
                            }
                            fclose(fptrA);
                            //** end sending A

                            //** Set up serverB soket
                            //codes from Beej's
                            int sockfd_serverB;
                            struct addrinfo hints_serverB, *servinfo_serverB, *p_serverB;
                            int rv_serverB;
                            //int numbytes_serverA;

                            memset(&hints_serverB, 0, sizeof hints_serverB);
                            hints_serverB.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverB.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverB = getaddrinfo("localhost", PORT_SERVERB, &hints_serverB, &servinfo_serverB)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverB));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverB = servinfo_serverB; p_serverB != NULL; p_serverB = p_serverB->ai_next)
                            {
                                if ((sockfd_serverB = socket(p_serverB->ai_family, p_serverB->ai_socktype,
                                                             p_serverB->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverB == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverB);
                            //** End of setting up serverB socket
                            FILE *fptrB;
                            fptrB = (fopen("tmp.txt", "a"));
                            if (fptrB == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending B
                            int number_of_linesB = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesB, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufB[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesB; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufB, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufB);
                                fprintf(fptrB, "%s", line_bufB);
                            }
                            fclose(fptrB);
                            //** end sending B
                            //** Set up serverC soket
                            //codes from Beej's
                            int sockfd_serverC;
                            struct addrinfo hints_serverC, *servinfo_serverC, *p_serverC;
                            int rv_serverC;

                            memset(&hints_serverC, 0, sizeof hints_serverC);
                            hints_serverC.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverC.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverC = getaddrinfo("localhost", PORT_SERVERC, &hints_serverC, &servinfo_serverC)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverC));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverC = servinfo_serverC; p_serverC != NULL; p_serverC = p_serverC->ai_next)
                            {
                                if ((sockfd_serverC = socket(p_serverC->ai_family, p_serverC->ai_socktype,
                                                             p_serverC->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverC == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverC);
                            //** End of setting up serverC socket
                            FILE *fptrC;
                            fptrC = (fopen("tmp.txt", "a"));
                            if (fptrC == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending C
                            int number_of_linesC = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesC, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufC[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesC; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufC, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufC);
                                fprintf(fptrC, "%s", line_bufC);
                            }
                            fclose(fptrC);
                            //** end sending C
                            int number_total = number_of_linesA + number_of_linesB + number_of_linesC;

                            //line_size = getline(&line_buf, &line_buf_size, fptr_tmp);
                            FILE *fptr_ali;
                            fptr_ali = (fopen("alichain.txt", "w"));
                            if (fptr_ali == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }
                            for (int i = 1; i <= number_total; i++)
                            {
                                FILE *fptr_tmp;
                                fptr_tmp = (fopen("tmp.txt", "r"));
                                if (fptr_tmp == NULL)
                                {
                                    printf("Error!");
                                    exit(1);
                                }
                                char *line_buf = NULL;
                                size_t line_buf_size = 0;
                                ssize_t line_size;
                                for (int j = 1; j <= number_total; j++)
                                {

                                    line_size = getline(&line_buf, &line_buf_size, fptr_tmp);
                                    char line_buf_copy[MAXDATASIZE];

                                    strncpy(line_buf_copy, line_buf, sizeof(line_buf_copy));

                                    char *token = strtok(line_buf, " ");
                                    int seq = atoi(token);
                                    if (seq == i)
                                    {
                                        fprintf(fptr_ali, "%s", line_buf_copy);
                                        //printf("%s", line_buf_copy);
                                    }
                                }
                                fclose(fptr_tmp);

                                //printf("%s", line_buf);
                            }

                            fclose(fptr_ali);
                            remove("tmp.txt");
                            printf("The sorted file is up and ready\n");
                        }

                        else if (operation == 5)
                        {
                            printf("A STATS request has been received\n");
                            char sender[MAXDATASIZE];
                            if ((numbytes = recv(newfd, sender, MAXDATASIZE - 1, 0)) == -1)
                            {
                                perror("recv");
                                exit(1);
                            }
                            sender[numbytes] = '\0';
                            
                            //** Set up UDP soket
                            //codes from Beej's
                            int sockfd_udp;
                            struct addrinfo hints_udp, *servinfo_udp, *p_udp;
                            int rv_udp;
                            int numbytes_udp;
                            struct sockaddr_storage their_addr;
                            socklen_t addr_len;

                            memset(&hints_udp, 0, sizeof hints_udp);
                            hints_udp.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_udp.ai_socktype = SOCK_DGRAM;

                            if ((rv_udp = getaddrinfo("localhost", UDP_PORT, &hints_udp, &servinfo_udp)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo udp: %s\n", gai_strerror(rv_udp));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_udp = servinfo_udp; p_udp != NULL; p_udp = p_udp->ai_next)
                            {
                                if ((sockfd_udp = socket(p_udp->ai_family, p_udp->ai_socktype,
                                                         p_udp->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_udp == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_udp);
                            //** End of setting up UDP socket
                            //** Set up serverA soket
                            //codes from Beej's
                            int sockfd_serverA;
                            struct addrinfo hints_serverA, *servinfo_serverA, *p_serverA;
                            int rv_serverA;
                            //int numbytes_serverA;

                            memset(&hints_serverA, 0, sizeof hints_serverA);
                            hints_serverA.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverA.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverA = getaddrinfo("localhost", PORT_SERVERA, &hints_serverA, &servinfo_serverA)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverA));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverA = servinfo_serverA; p_serverA != NULL; p_serverA = p_serverA->ai_next)
                            {
                                if ((sockfd_serverA = socket(p_serverA->ai_family, p_serverA->ai_socktype,
                                                             p_serverA->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverA == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverA);
                            //** End of setting up serverA socket

                            FILE *fptrA;
                            fptrA = (fopen("tmp2.txt", "w"));
                            if (fptrA == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending A
                            int number_of_linesA = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesA, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufA[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesA; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufA, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufA);
                                fprintf(fptrA, "%s", line_bufA);
                            }
                            fclose(fptrA);
                            //** end sending A

                            //** Set up serverB soket
                            //codes from Beej's
                            int sockfd_serverB;
                            struct addrinfo hints_serverB, *servinfo_serverB, *p_serverB;
                            int rv_serverB;
                            //int numbytes_serverA;

                            memset(&hints_serverB, 0, sizeof hints_serverB);
                            hints_serverB.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverB.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverB = getaddrinfo("localhost", PORT_SERVERB, &hints_serverB, &servinfo_serverB)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverB));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverB = servinfo_serverB; p_serverB != NULL; p_serverB = p_serverB->ai_next)
                            {
                                if ((sockfd_serverB = socket(p_serverB->ai_family, p_serverB->ai_socktype,
                                                             p_serverB->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverB == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverB);
                            //** End of setting up serverB socket
                            FILE *fptrB;
                            fptrB = (fopen("tmp2.txt", "a"));
                            if (fptrB == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending B
                            int number_of_linesB = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesB, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufB[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesB; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufB, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufB);
                                fprintf(fptrB, "%s", line_bufB);
                            }
                            fclose(fptrB);
                            //** end sending B
                            //** Set up serverC soket
                            //codes from Beej's
                            int sockfd_serverC;
                            struct addrinfo hints_serverC, *servinfo_serverC, *p_serverC;
                            int rv_serverC;

                            memset(&hints_serverC, 0, sizeof hints_serverC);
                            hints_serverC.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverC.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverC = getaddrinfo("localhost", PORT_SERVERC, &hints_serverC, &servinfo_serverC)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverC));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverC = servinfo_serverC; p_serverC != NULL; p_serverC = p_serverC->ai_next)
                            {
                                if ((sockfd_serverC = socket(p_serverC->ai_family, p_serverC->ai_socktype,
                                                             p_serverC->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverC == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverC);
                            //** End of setting up serverC socket
                            FILE *fptrC;
                            fptrC = (fopen("tmp2.txt", "a"));
                            if (fptrC == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending C
                            int number_of_linesC = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesC, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufC[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesC; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufC, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufC);
                                fprintf(fptrC, "%s", line_bufC);
                            }
                            fclose(fptrC);
                            //** end sending C
                            int number_total = number_of_linesA + number_of_linesB + number_of_linesC;

                            //line_size = getline(&line_buf, &line_buf_size, fptr_tmp);
                            FILE *fptr_usr;
                            fptr_usr = (fopen("usr.txt", "w"));
                            if (fptr_usr == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            FILE *fptr_tmp;
                            fptr_tmp = (fopen("tmp2.txt", "r"));
                            if (fptr_tmp == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }
                            char *line_buf = NULL;
                            size_t line_buf_size = 0;
                            ssize_t line_size;
                            int cnt = 0;
                            for (int j = 1; j <= number_total; j++)
                            {

                                line_size = getline(&line_buf, &line_buf_size, fptr_tmp);
                                char line_buf_copy[MAXDATASIZE];
                                strncpy(line_buf_copy, line_buf, sizeof(line_buf_copy));

                                char *token = strtok(line_buf, " ");
                                int seq = atoi(token);

                                bool isSender = false;
                                bool isReceiver = false;
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) == 0)
                                {
                                    isSender = true;
                                }

                                //printf(" %s\n", token); //printing each token

                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) == 0)
                                {
                                    isReceiver = true;
                                }

                                //printf(" %s\n", token); //printing each token
                                if (isSender || isReceiver)
                                {
                                    fprintf(fptr_usr, "%s", line_buf_copy);
                                    cnt++;
                                }
                            }
                            fclose(fptr_tmp);

                            fclose(fptr_usr);
                            remove("tmp2.txt");

                            FILE *fptr_usr1 = (fopen("usr.txt", "r"));
                            line_buf = NULL;
                            line_buf_size = 0;
                            FILE *fptr_usrname = (fopen("usrname.txt", "w"));
                            char **str = (char **)malloc(sizeof(char *) * cnt);
                            for (int i = 0; i < cnt; i++)
                            {
                                str[i] = (char *)malloc(sizeof(char) * MAXDATASIZE);
                            }
                            int usrcnt = 0;
                            for (int j = 0; j < cnt; j++)
                            {
                                line_size = getline(&line_buf, &line_buf_size, fptr_usr1);
                                char line_buf_copy[MAXDATASIZE];
                                strncpy(line_buf_copy, line_buf, sizeof(line_buf_copy));
                                //printf("%s",line_buf);
                                char *token = strtok(line_buf_copy, " ");
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {
                                    bool usrexist = false;
                                    for (int i = 0; i < cnt; i++)
                                    {
                                        if (strcmp(token, str[i]) == 0)
                                            usrexist = true;
                                    }
                                    if (!usrexist)
                                    {
                                        strcpy(str[j], token);
                                        //printf("%s\n",str[j]);
                                        fprintf(fptr_usrname,"%s\n",str[j]);
                                        usrcnt++;
                                    }
                                }
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {
                                    bool usrexist = false;
                                    for (int i = 0; i < cnt; i++)
                                    {
                                        if (strcmp(token, str[i]) == 0)
                                            usrexist = true;
                                    }
                                    if (!usrexist)
                                    {
                                        strcpy(str[j], token);
                                        //printf("%s\n",str[j]);
                                        fprintf(fptr_usrname,"%s\n",str[j]);
                                        usrcnt++;
                                    }
                                }
                            }
                            
                            fclose(fptr_usr1);
                            fclose(fptr_usrname);
                            for (int i = 0; i < cnt; i++)
                            {
                                free(str[i]);
                            }
                            free(str);

                            char **usrname = (char **)malloc(sizeof(char *) * usrcnt);
                            for (int i = 0; i < usrcnt; i++)
                            {
                                usrname[i] = (char *)malloc(sizeof(char) * MAXDATASIZE);
                            }

                            char **str2 = (char **)malloc(sizeof(char *) * cnt);
                            for (int i = 0; i < cnt; i++)
                            {
                                str2[i] = (char *)malloc(sizeof(char) * MAXDATASIZE);
                            }
                            FILE *fptr_usr2 = (fopen("usr.txt", "r"));
                            line_buf = NULL;
                            line_buf_size = 0;
                            int usrindex = 0;
                            for (int j = 0; j < cnt; j++)
                            {
                                line_size = getline(&line_buf, &line_buf_size, fptr_usr2);
                                char line_buf_copy[MAXDATASIZE];
                                strncpy(line_buf_copy, line_buf, sizeof(line_buf_copy));

                                char *token = strtok(line_buf_copy, " ");
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {
                                    bool usrexist = false;
                                    for (int i = 0; i < cnt; i++)
                                    {
                                        if (strcmp(token, str2[i]) == 0)
                                            usrexist = true;
                                    }
                                    if (!usrexist)
                                    {
                                        strcpy(str2[j], token);
                                        strcpy(usrname[usrindex], token);
                                        usrindex++;
                                    }
                                }
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {
                                    bool usrexist = false;
                                    for (int i = 0; i < cnt; i++)
                                    {
                                        if (strcmp(token, str2[i]) == 0)
                                            usrexist = true;
                                    }
                                    if (!usrexist)
                                    {
                                        strcpy(str2[j], token);
                                        strcpy(usrname[usrindex], token);
                                        usrindex++;
                                    }
                                }
                            }
                            fclose(fptr_usr2);
                            
                            /*for (int i = 0; i < usrcnt; i++)
                            {
                                printf("%s ", usrname[i]);
                            }
                            printf("\n");*/
                            FILE *fptr_usr3 = (fopen("usr.txt", "r"));
                            line_buf = NULL;
                            line_buf_size = 0;
                            int *cntTrans = (int *)calloc(usrcnt, sizeof(int));
                            for (int i = 0; i < usrcnt; i++)
                            {
                                cntTrans[i] = 0;
                            }
                            int *amtTrans = (int *)calloc(usrcnt, sizeof(int));
                            for (int i = 0; i < usrcnt; i++)
                            {
                                amtTrans[i] = 0;
                            }
                            for (int j = 0; j < cnt; j++)
                            {
                                line_size = getline(&line_buf, &line_buf_size, fptr_usr3);
                                char line_buf_copy[MAXDATASIZE];
                                strncpy(line_buf_copy, line_buf, sizeof(line_buf_copy));

                                bool isSender = false;
                                bool isReceiver = false;
                                char *token = strtok(line_buf_copy, " ");
                                int index;
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {

                                    for (int i = 0; i < usrcnt; i++)
                                    {
                                        if (strcmp(token, usrname[i]) == 0)
                                            index = i;
                                    }
                                    cntTrans[index]++;
                                    isSender = true;
                                }
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {

                                    for (int i = 0; i < usrcnt; i++)
                                    {
                                        if (strcmp(token, usrname[i]) == 0)
                                            index = i;
                                    }
                                    cntTrans[index]++;
                                    isReceiver = true;
                                }
                                token = strtok(NULL, " ");
                                int curAmt = atoi(token);
                                if (isSender)
                                    amtTrans[index] -= curAmt;
                                else if (isReceiver)
                                    amtTrans[index] += curAmt;
                            }
                            fclose(fptr_usr3);
                           
                            
                            //bubble sort
                            int tmp_cnt,tmp_amt;
                            char *tmp_name = (char *)malloc(sizeof(char) * MAXDATASIZE);
                            for (int i = 0; i < usrcnt - 1; i++){
                                for (int j = 0; j < usrcnt - 1 - i; j++){
                                    if (cntTrans[j] < cntTrans[j + 1])
                                    {
                                        tmp_cnt = cntTrans[j];
                                        cntTrans[j] = cntTrans[j + 1];
                                        cntTrans[j + 1] = tmp_cnt;
                                        tmp_amt = amtTrans[j];
                                        amtTrans[j] = amtTrans[j + 1];
                                        amtTrans[j + 1] = tmp_amt;
                                        strcpy(tmp_name,usrname[j]);
                                        strcpy(usrname[j],usrname[j+1]);
                                        strcpy(usrname[j+1],tmp_name);

                                    }
                                }
                                    
                            }
                            
                            if (send(newfd, &usrcnt, sizeof(int), 0) == -1)
                                perror("send");

                            for (int i=0;i<usrcnt;i++){
                                if (send(newfd, usrname[i], MAXDATASIZE-1, 0) == -1)
                                    perror("send");
                                if (send(newfd, cntTrans+i, sizeof(int), 0) == -1)
                                    perror("send");
                                if (send(newfd, amtTrans+i, sizeof(int), 0) == -1)
                                    perror("send");
                            }
                            remove("usr.txt");
                            remove("usrname.txt");

                                
                            

                            /*char *lbc;
                                lbc = (char*)malloc(sizeof(char)*MAXDATASIZE);  
                                strcpy(lbc, line_buf);
                                printf("%s",lbc); 
                                free(lbc);*/
                        }
                    }
                }
                else if (pfds[i].fd == listener_cB)
                {
                    // If listener_cA is ready to read, handle new connection

                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener_cB,
                                   (struct sockaddr *)&remoteaddr,
                                   &addrlen);

                    if (newfd == -1)
                    {
                        perror("accept");
                    }
                    else
                    {
                        add_to_pfds(&pfds, newfd, &fd_count, &fd_size);

                        if ((numbytes = recv(newfd, &operation, sizeof(int), 0)) == -1)
                        {
                            perror("recv");
                            exit(1);
                        }
                        //printf("%d\n",operation);
                        if (operation == 1)
                        {
                            //** Set up UDP soket
                            //codes from Beej's
                            int sockfd_udp;
                            struct addrinfo hints_udp, *servinfo_udp, *p_udp;
                            int rv_udp;
                            int numbytes_udp;
                            struct sockaddr_storage their_addr;
                            socklen_t addr_len;

                            memset(&hints_udp, 0, sizeof hints_udp);
                            hints_udp.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_udp.ai_socktype = SOCK_DGRAM;

                            if ((rv_udp = getaddrinfo("localhost", UDP_PORT, &hints_udp, &servinfo_udp)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo udp: %s\n", gai_strerror(rv_udp));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_udp = servinfo_udp; p_udp != NULL; p_udp = p_udp->ai_next)
                            {
                                if ((sockfd_udp = socket(p_udp->ai_family, p_udp->ai_socktype,
                                                         p_udp->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_udp == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_udp);
                            //** End of setting up UDP socket
                            //** Set up serverA soket
                            //codes from Beej's
                            int sockfd_serverA;
                            struct addrinfo hints_serverA, *servinfo_serverA, *p_serverA;
                            int rv_serverA;
                            //int numbytes_serverA;

                            memset(&hints_serverA, 0, sizeof hints_serverA);
                            hints_serverA.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverA.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverA = getaddrinfo("localhost", PORT_SERVERA, &hints_serverA, &servinfo_serverA)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverA));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverA = servinfo_serverA; p_serverA != NULL; p_serverA = p_serverA->ai_next)
                            {
                                if ((sockfd_serverA = socket(p_serverA->ai_family, p_serverA->ai_socktype,
                                                             p_serverA->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverA == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverA);
                            //** End of setting up serverA socket

                            //printf("%d\n",operation);
                            if ((numbytes = recv(newfd, buf, MAXDATASIZE - 1, 0)) == -1)
                            {
                                perror("recv");
                                exit(1);
                            }
                            buf[numbytes] = '\0';
                            printf("The main server received input =\"%s\" from the client using TCP over port %s\n", buf, TCP_PORT_CLIENTB);

                            //***sending A
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, buf, MAXDATASIZE - 1, 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server A\n");
                            addr_len = sizeof their_addr;
                            int balance_changeA = 0;
                            bool isInNetworkA = false;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkA, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeA, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            printf("The main server received transactions from Server A using UDP over port %s\n", PORT_SERVERA);
                            //end sending A

                            //** Set up serverB soket
                            //codes from Beej's
                            int sockfd_serverB;
                            struct addrinfo hints_serverB, *servinfo_serverB, *p_serverB;
                            int rv_serverB;
                            //int numbytes_serverA;

                            memset(&hints_serverB, 0, sizeof hints_serverB);
                            hints_serverB.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverB.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverB = getaddrinfo("localhost", PORT_SERVERB, &hints_serverB, &servinfo_serverB)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverB));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverB = servinfo_serverB; p_serverB != NULL; p_serverB = p_serverB->ai_next)
                            {
                                if ((sockfd_serverB = socket(p_serverB->ai_family, p_serverB->ai_socktype,
                                                             p_serverB->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverB == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverB);
                            //** End of setting up serverB socket
                            //***sending B
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, buf, MAXDATASIZE - 1, 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server B\n");
                            addr_len = sizeof their_addr;
                            int balance_changeB = 0;
                            bool isInNetworkB = false;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkB, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeB, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            printf("The main server received transactions from Server B using UDP over port %s\n", PORT_SERVERB);
                            //end sending B
                            //** Set up serverC soket
                            //codes from Beej's
                            int sockfd_serverC;
                            struct addrinfo hints_serverC, *servinfo_serverC, *p_serverC;
                            int rv_serverC;

                            memset(&hints_serverC, 0, sizeof hints_serverC);
                            hints_serverC.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverC.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverC = getaddrinfo("localhost", PORT_SERVERC, &hints_serverC, &servinfo_serverC)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverC));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverC = servinfo_serverC; p_serverC != NULL; p_serverC = p_serverC->ai_next)
                            {
                                if ((sockfd_serverC = socket(p_serverC->ai_family, p_serverC->ai_socktype,
                                                             p_serverC->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverC == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverC);
                            //** End of setting up serverC socket
                            //***sending C
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, buf, MAXDATASIZE - 1, 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server C\n");
                            addr_len = sizeof their_addr;
                            int balance_changeC = 0;
                            bool isInNetworkC = false;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkC, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeC, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            printf("The main server received transactions from Server C using UDP over port %s\n", PORT_SERVERC);
                            //end sending C

                            int balance = -1;
                            if (isInNetworkA || isInNetworkB || isInNetworkC)
                            {
                                balance = 1000 + balance_changeA + balance_changeB + balance_changeC;
                            }
                            if (send(newfd, &balance, sizeof(int), 0) == -1)
                                perror("send");
                            printf("The main server sent the current balance to client B\n");
                        }

                        else if (operation == 2)
                        {
                            //** Set up UDP soket
                            //codes from Beej's
                            int sockfd_udp;
                            struct addrinfo hints_udp, *servinfo_udp, *p_udp;
                            int rv_udp;
                            int numbytes_udp;
                            struct sockaddr_storage their_addr;
                            socklen_t addr_len;

                            memset(&hints_udp, 0, sizeof hints_udp);
                            hints_udp.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_udp.ai_socktype = SOCK_DGRAM;

                            if ((rv_udp = getaddrinfo("localhost", UDP_PORT, &hints_udp, &servinfo_udp)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo udp: %s\n", gai_strerror(rv_udp));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_udp = servinfo_udp; p_udp != NULL; p_udp = p_udp->ai_next)
                            {
                                if ((sockfd_udp = socket(p_udp->ai_family, p_udp->ai_socktype,
                                                         p_udp->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_udp == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_udp);
                            //** End of setting up UDP socket
                            //** Set up serverA soket
                            //codes from Beej's
                            int sockfd_serverA;
                            struct addrinfo hints_serverA, *servinfo_serverA, *p_serverA;
                            int rv_serverA;
                            //int numbytes_serverA;

                            memset(&hints_serverA, 0, sizeof hints_serverA);
                            hints_serverA.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverA.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverA = getaddrinfo("localhost", PORT_SERVERA, &hints_serverA, &servinfo_serverA)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverA));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverA = servinfo_serverA; p_serverA != NULL; p_serverA = p_serverA->ai_next)
                            {
                                if ((sockfd_serverA = socket(p_serverA->ai_family, p_serverA->ai_socktype,
                                                             p_serverA->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverA == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverA);
                            //** End of setting up serverA socket

                            //codes from Beej's
                            //printf("%d\n",operation);
                            char sender[MAXDATASIZE];
                            char receiver[MAXDATASIZE];
                            char amountstr[MAXDATASIZE];
                            //printf("%d\n",operation);
                            if ((numbytes = recv(newfd, sender, MAXDATASIZE - 1, 0)) == -1)
                            {
                                perror("recv");
                                exit(1);
                            }
                            sender[numbytes] = '\0';

                            if ((numbytes = recv(newfd, receiver, MAXDATASIZE - 1, 0)) == -1)
                            {
                                perror("recv");
                                exit(1);
                            }
                            receiver[numbytes] = '\0';

                            if ((numbytes = recv(newfd, amountstr, MAXDATASIZE - 1, 0)) == -1)
                            {
                                perror("recv");
                                exit(1);
                            }
                            amountstr[numbytes] = '\0';

                            printf("The main server received from %s to transfer %s coins to %s using TCP over port %s\n", sender, amountstr, receiver, TCP_PORT_CLIENTB);

                            //** sending A
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, sender, MAXDATASIZE - 1, 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, receiver, MAXDATASIZE - 1, 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server A\n");

                            addr_len = sizeof their_addr;
                            int balance_changeA = 0;
                            bool isInNetworkA = false;
                            bool isInNetworkA_rec = false;
                            int maxSeqA = 0;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkA, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkA_rec, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeA, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &maxSeqA, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            printf("The main server received the feedback from Server A using UDP over port %s\n", PORT_SERVERA);
                            //**end sending A
                            //** Set up serverB soket
                            //codes from Beej's
                            int sockfd_serverB;
                            struct addrinfo hints_serverB, *servinfo_serverB, *p_serverB;
                            int rv_serverB;
                            //int numbytes_serverA;

                            memset(&hints_serverB, 0, sizeof hints_serverB);
                            hints_serverB.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverB.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverB = getaddrinfo("localhost", PORT_SERVERB, &hints_serverB, &servinfo_serverB)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverB));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverB = servinfo_serverB; p_serverB != NULL; p_serverB = p_serverB->ai_next)
                            {
                                if ((sockfd_serverB = socket(p_serverB->ai_family, p_serverB->ai_socktype,
                                                             p_serverB->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverB == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverB);
                            //** End of setting up serverB socket
                            //***sending B
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, sender, MAXDATASIZE - 1, 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, receiver, MAXDATASIZE - 1, 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server B\n");
                            addr_len = sizeof their_addr;
                            int balance_changeB = 0;
                            bool isInNetworkB = false;
                            bool isInNetworkB_rec = false;
                            int maxSeqB = 0;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkB, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkB_rec, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeB, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &maxSeqB, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            printf("The main server received the feedback from Server B using UDP over port %s\n", PORT_SERVERB);
                            //end sending B
                            //** Set up serverC soket
                            //codes from Beej's
                            int sockfd_serverC;
                            struct addrinfo hints_serverC, *servinfo_serverC, *p_serverC;
                            int rv_serverC;

                            memset(&hints_serverC, 0, sizeof hints_serverC);
                            hints_serverC.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverC.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverC = getaddrinfo("localhost", PORT_SERVERC, &hints_serverC, &servinfo_serverC)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverC));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverC = servinfo_serverC; p_serverC != NULL; p_serverC = p_serverC->ai_next)
                            {
                                if ((sockfd_serverC = socket(p_serverC->ai_family, p_serverC->ai_socktype,
                                                             p_serverC->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverC == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverC);
                            //** End of setting up serverC socket
                            //***sending C
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, sender, MAXDATASIZE - 1, 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = sendto(sockfd_udp, receiver, MAXDATASIZE - 1, 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            printf("The main server sent a request to Server C\n");
                            addr_len = sizeof their_addr;
                            int balance_changeC = 0;
                            bool isInNetworkC = false;
                            bool isInNetworkC_rec = false;
                            int maxSeqC = 0;
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkC, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &isInNetworkC_rec, sizeof(bool), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &balance_changeC, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &maxSeqC, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            printf("The main server received the feedback from Server C using UDP over port %s\n", PORT_SERVERC);
                            //end sending C

                            //printf("maxSeqA: %d\n",maxSeqA);
                            //printf("maxSeqB: %d\n",maxSeqB);
                            //printf("maxSeqC: %d\n",maxSeqC);
                            int curSeq = maxSeqA;
                            if (maxSeqB > curSeq)
                            {
                                curSeq = maxSeqB;
                            }
                            if (maxSeqC > curSeq)
                            {
                                curSeq = maxSeqC;
                            }
                            //printf("curSeq: %d\n", curSeq);
                            int balance2 = -1;
                            bool isSenderInNetwork = false;
                            bool isReceiverInNetwork = false;
                            int transamount = atoi(amountstr);
                            //printf("%d\n", transamount);
                            if (isInNetworkA || isInNetworkB || isInNetworkC)
                            {
                                balance2 = 1000 + balance_changeA + balance_changeB + balance_changeC;
                                isSenderInNetwork = true;
                            }
                            if (isInNetworkA_rec || isInNetworkB_rec || isInNetworkC_rec)
                            {
                                isReceiverInNetwork = true;
                            }

                            int status = 0; //1 for seccess,2 for insufficent balance2, 3 for sender out network,4 for receiver out network, 5 for 2 out network
                            if (isSenderInNetwork && isReceiverInNetwork)
                            {
                                if (balance2 >= transamount)
                                {
                                    status = 1;
                                    balance2 -= transamount;
                                }
                                else
                                {
                                    status = 2;
                                }
                            }
                            else if (isSenderInNetwork)
                            {
                                status = 4;
                            }
                            else if (isReceiverInNetwork)
                            {
                                status = 3;
                            }
                            else
                            {
                                status = 5;
                            }
                            //***write to logfile
                            if (status == 1)
                            {
                                int r = rand() % 3;
                                //printf("random number:%d\n", r);
                                int numrandom = r + 1;
                                char *currentPort;
                                if (numrandom == 1)
                                {
                                    currentPort = PORT_SERVERA;
                                }
                                else if (numrandom == 2)
                                {
                                    currentPort = PORT_SERVERB;
                                }
                                else
                                {
                                    currentPort = PORT_SERVERC;
                                }
                                //** Set up wt soket
                                //codes from Beej's
                                int sockfd_wt;
                                struct addrinfo hints_wt, *servinfo_wt, *p_wt;
                                int rv_wt;

                                memset(&hints_wt, 0, sizeof hints_wt);
                                hints_wt.ai_family = AF_INET6; // set to AF_INET to use IPv4
                                hints_wt.ai_socktype = SOCK_DGRAM;

                                if ((rv_wt = getaddrinfo("localhost", currentPort, &hints_wt, &servinfo_wt)) != 0)
                                {
                                    fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_wt));
                                    return 1;
                                }

                                // loop through all the results and make a socket
                                for (p_wt = servinfo_wt; p_wt != NULL; p_wt = p_wt->ai_next)
                                {
                                    if ((sockfd_wt = socket(p_wt->ai_family, p_wt->ai_socktype,
                                                            p_wt->ai_protocol)) == -1)
                                    {
                                        perror("talker: socket");
                                        continue;
                                    }

                                    break;
                                }

                                if (p_wt == NULL)
                                {
                                    fprintf(stderr, "talker: failed to create socket\n");
                                    return 2;
                                }
                                freeaddrinfo(servinfo_wt);
                                //** End of setting up wt socket
                                int tmp_operation = 3;
                                if ((numbytes_udp = sendto(sockfd_udp, &tmp_operation, sizeof(int), 0,
                                                           p_wt->ai_addr, p_wt->ai_addrlen)) == -1)
                                {
                                    perror("talker: sendto 1");
                                    exit(1);
                                }

                                int newSeq = curSeq + 1;
                                if ((numbytes_udp = sendto(sockfd_udp, &newSeq, sizeof(int), 0,
                                                           p_wt->ai_addr, p_wt->ai_addrlen)) == -1)
                                {
                                    perror("talker: sendto 1");
                                    exit(1);
                                }
                                if ((numbytes_udp = sendto(sockfd_udp, sender, MAXDATASIZE - 1, 0,
                                                           p_wt->ai_addr, p_wt->ai_addrlen)) == -1)
                                {
                                    perror("talker: sendto 1");
                                    exit(1);
                                }
                                if ((numbytes_udp = sendto(sockfd_udp, receiver, MAXDATASIZE - 1, 0,
                                                           p_wt->ai_addr, p_wt->ai_addrlen)) == -1)
                                {
                                    perror("talker: sendto 1");
                                    exit(1);
                                }
                                if ((numbytes_udp = sendto(sockfd_udp, &transamount, sizeof(int), 0,
                                                           p_wt->ai_addr, p_wt->ai_addrlen)) == -1)
                                {
                                    perror("talker: sendto 1");
                                    exit(1);
                                }
                            }
                            //***end of writing to logfile

                            if (send(newfd, &status, sizeof(int), 0) == -1)
                                perror("send");

                            if (send(newfd, &balance2, sizeof(int), 0) == -1)
                                perror("send");
                            printf("The main server sent the result of transaction to client B\n");
                        }

                        else if (operation == 4)
                        {
                            printf("A TXLIST request has been received\n");
                            //** Set up UDP soket
                            //codes from Beej's
                            int sockfd_udp;
                            struct addrinfo hints_udp, *servinfo_udp, *p_udp;
                            int rv_udp;
                            int numbytes_udp;
                            struct sockaddr_storage their_addr;
                            socklen_t addr_len;

                            memset(&hints_udp, 0, sizeof hints_udp);
                            hints_udp.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_udp.ai_socktype = SOCK_DGRAM;

                            if ((rv_udp = getaddrinfo("localhost", UDP_PORT, &hints_udp, &servinfo_udp)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo udp: %s\n", gai_strerror(rv_udp));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_udp = servinfo_udp; p_udp != NULL; p_udp = p_udp->ai_next)
                            {
                                if ((sockfd_udp = socket(p_udp->ai_family, p_udp->ai_socktype,
                                                         p_udp->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_udp == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_udp);
                            //** End of setting up UDP socket
                            //** Set up serverA soket
                            //codes from Beej's
                            int sockfd_serverA;
                            struct addrinfo hints_serverA, *servinfo_serverA, *p_serverA;
                            int rv_serverA;
                            //int numbytes_serverA;

                            memset(&hints_serverA, 0, sizeof hints_serverA);
                            hints_serverA.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverA.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverA = getaddrinfo("localhost", PORT_SERVERA, &hints_serverA, &servinfo_serverA)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverA));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverA = servinfo_serverA; p_serverA != NULL; p_serverA = p_serverA->ai_next)
                            {
                                if ((sockfd_serverA = socket(p_serverA->ai_family, p_serverA->ai_socktype,
                                                             p_serverA->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverA == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverA);
                            //** End of setting up serverA socket

                            FILE *fptrA;
                            fptrA = (fopen("tmp.txt", "w"));
                            if (fptrA == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending A
                            int number_of_linesA = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesA, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufA[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesA; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufA, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufA);
                                fprintf(fptrA, "%s", line_bufA);
                            }
                            fclose(fptrA);
                            //** end sending A

                            //** Set up serverB soket
                            //codes from Beej's
                            int sockfd_serverB;
                            struct addrinfo hints_serverB, *servinfo_serverB, *p_serverB;
                            int rv_serverB;
                            //int numbytes_serverA;

                            memset(&hints_serverB, 0, sizeof hints_serverB);
                            hints_serverB.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverB.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverB = getaddrinfo("localhost", PORT_SERVERB, &hints_serverB, &servinfo_serverB)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverB));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverB = servinfo_serverB; p_serverB != NULL; p_serverB = p_serverB->ai_next)
                            {
                                if ((sockfd_serverB = socket(p_serverB->ai_family, p_serverB->ai_socktype,
                                                             p_serverB->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverB == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverB);
                            //** End of setting up serverB socket
                            FILE *fptrB;
                            fptrB = (fopen("tmp.txt", "a"));
                            if (fptrB == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending B
                            int number_of_linesB = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesB, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufB[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesB; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufB, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufB);
                                fprintf(fptrB, "%s", line_bufB);
                            }
                            fclose(fptrB);
                            //** end sending B
                            //** Set up serverC soket
                            //codes from Beej's
                            int sockfd_serverC;
                            struct addrinfo hints_serverC, *servinfo_serverC, *p_serverC;
                            int rv_serverC;

                            memset(&hints_serverC, 0, sizeof hints_serverC);
                            hints_serverC.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverC.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverC = getaddrinfo("localhost", PORT_SERVERC, &hints_serverC, &servinfo_serverC)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverC));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverC = servinfo_serverC; p_serverC != NULL; p_serverC = p_serverC->ai_next)
                            {
                                if ((sockfd_serverC = socket(p_serverC->ai_family, p_serverC->ai_socktype,
                                                             p_serverC->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverC == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverC);
                            //** End of setting up serverC socket
                            FILE *fptrC;
                            fptrC = (fopen("tmp.txt", "a"));
                            if (fptrC == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending C
                            int number_of_linesC = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesC, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufC[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesC; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufC, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufC);
                                fprintf(fptrC, "%s", line_bufC);
                            }
                            fclose(fptrC);
                            //** end sending C
                            int number_total = number_of_linesA + number_of_linesB + number_of_linesC;

                            //line_size = getline(&line_buf, &line_buf_size, fptr_tmp);
                            FILE *fptr_ali;
                            fptr_ali = (fopen("alichain.txt", "w"));
                            if (fptr_ali == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }
                            for (int i = 1; i <= number_total; i++)
                            {
                                FILE *fptr_tmp;
                                fptr_tmp = (fopen("tmp.txt", "r"));
                                if (fptr_tmp == NULL)
                                {
                                    printf("Error!");
                                    exit(1);
                                }
                                char *line_buf = NULL;
                                size_t line_buf_size = 0;
                                ssize_t line_size;
                                for (int j = 1; j <= number_total; j++)
                                {

                                    line_size = getline(&line_buf, &line_buf_size, fptr_tmp);
                                    char line_buf_copy[MAXDATASIZE];

                                    strncpy(line_buf_copy, line_buf, sizeof(line_buf_copy));

                                    char *token = strtok(line_buf, " ");
                                    int seq = atoi(token);
                                    if (seq == i)
                                    {
                                        fprintf(fptr_ali, "%s", line_buf_copy);
                                        //printf("%s", line_buf_copy);
                                    }
                                }
                                fclose(fptr_tmp);

                                //printf("%s", line_buf);
                            }

                            fclose(fptr_ali);
                            remove("tmp.txt");
                            printf("The sorted file is up and ready\n");
                        }

                        else if (operation == 5)
                        {
                            printf("A STATS request has been received\n");
                            char sender[MAXDATASIZE];
                            if ((numbytes = recv(newfd, sender, MAXDATASIZE - 1, 0)) == -1)
                            {
                                perror("recv");
                                exit(1);
                            }
                            sender[numbytes] = '\0';
                            
                            //** Set up UDP soket
                            //codes from Beej's
                            int sockfd_udp;
                            struct addrinfo hints_udp, *servinfo_udp, *p_udp;
                            int rv_udp;
                            int numbytes_udp;
                            struct sockaddr_storage their_addr;
                            socklen_t addr_len;

                            memset(&hints_udp, 0, sizeof hints_udp);
                            hints_udp.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_udp.ai_socktype = SOCK_DGRAM;

                            if ((rv_udp = getaddrinfo("localhost", UDP_PORT, &hints_udp, &servinfo_udp)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo udp: %s\n", gai_strerror(rv_udp));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_udp = servinfo_udp; p_udp != NULL; p_udp = p_udp->ai_next)
                            {
                                if ((sockfd_udp = socket(p_udp->ai_family, p_udp->ai_socktype,
                                                         p_udp->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_udp == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_udp);
                            //** End of setting up UDP socket
                            //** Set up serverA soket
                            //codes from Beej's
                            int sockfd_serverA;
                            struct addrinfo hints_serverA, *servinfo_serverA, *p_serverA;
                            int rv_serverA;
                            //int numbytes_serverA;

                            memset(&hints_serverA, 0, sizeof hints_serverA);
                            hints_serverA.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverA.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverA = getaddrinfo("localhost", PORT_SERVERA, &hints_serverA, &servinfo_serverA)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverA));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverA = servinfo_serverA; p_serverA != NULL; p_serverA = p_serverA->ai_next)
                            {
                                if ((sockfd_serverA = socket(p_serverA->ai_family, p_serverA->ai_socktype,
                                                             p_serverA->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverA == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverA);
                            //** End of setting up serverA socket

                            FILE *fptrA;
                            fptrA = (fopen("tmp2.txt", "w"));
                            if (fptrA == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending A
                            int number_of_linesA = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverA->ai_addr, p_serverA->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesA, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufA[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesA; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufA, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufA);
                                fprintf(fptrA, "%s", line_bufA);
                            }
                            fclose(fptrA);
                            //** end sending A

                            //** Set up serverB soket
                            //codes from Beej's
                            int sockfd_serverB;
                            struct addrinfo hints_serverB, *servinfo_serverB, *p_serverB;
                            int rv_serverB;
                            //int numbytes_serverA;

                            memset(&hints_serverB, 0, sizeof hints_serverB);
                            hints_serverB.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverB.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverB = getaddrinfo("localhost", PORT_SERVERB, &hints_serverB, &servinfo_serverB)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverB));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverB = servinfo_serverB; p_serverB != NULL; p_serverB = p_serverB->ai_next)
                            {
                                if ((sockfd_serverB = socket(p_serverB->ai_family, p_serverB->ai_socktype,
                                                             p_serverB->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverB == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverB);
                            //** End of setting up serverB socket
                            FILE *fptrB;
                            fptrB = (fopen("tmp2.txt", "a"));
                            if (fptrB == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending B
                            int number_of_linesB = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverB->ai_addr, p_serverB->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesB, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufB[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesB; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufB, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufB);
                                fprintf(fptrB, "%s", line_bufB);
                            }
                            fclose(fptrB);
                            //** end sending B
                            //** Set up serverC soket
                            //codes from Beej's
                            int sockfd_serverC;
                            struct addrinfo hints_serverC, *servinfo_serverC, *p_serverC;
                            int rv_serverC;

                            memset(&hints_serverC, 0, sizeof hints_serverC);
                            hints_serverC.ai_family = AF_INET6; // set to AF_INET to use IPv4
                            hints_serverC.ai_socktype = SOCK_DGRAM;

                            if ((rv_serverC = getaddrinfo("localhost", PORT_SERVERC, &hints_serverC, &servinfo_serverC)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo sA: %s\n", gai_strerror(rv_serverC));
                                return 1;
                            }

                            // loop through all the results and make a socket
                            for (p_serverC = servinfo_serverC; p_serverC != NULL; p_serverC = p_serverC->ai_next)
                            {
                                if ((sockfd_serverC = socket(p_serverC->ai_family, p_serverC->ai_socktype,
                                                             p_serverC->ai_protocol)) == -1)
                                {
                                    perror("talker: socket");
                                    continue;
                                }

                                break;
                            }

                            if (p_serverC == NULL)
                            {
                                fprintf(stderr, "talker: failed to create socket\n");
                                return 2;
                            }
                            freeaddrinfo(servinfo_serverC);
                            //** End of setting up serverC socket
                            FILE *fptrC;
                            fptrC = (fopen("tmp2.txt", "a"));
                            if (fptrC == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            //**sending C
                            int number_of_linesC = 0;
                            if ((numbytes_udp = sendto(sockfd_udp, &operation, sizeof(int), 0,
                                                       p_serverC->ai_addr, p_serverC->ai_addrlen)) == -1)
                            {
                                perror("talker: sendto 1");
                                exit(1);
                            }
                            if ((numbytes_udp = recvfrom(sockfd_udp, &number_of_linesC, sizeof(int), 0,
                                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }

                            char line_bufC[MAXDATASIZE];
                            for (int i = 0; i < number_of_linesC; i++)
                            {
                                if ((numbytes_udp = recvfrom(sockfd_udp, line_bufC, MAXDATASIZE - 1, 0,
                                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }
                                //printf("%s", line_bufC);
                                fprintf(fptrC, "%s", line_bufC);
                            }
                            fclose(fptrC);
                            //** end sending C
                            int number_total = number_of_linesA + number_of_linesB + number_of_linesC;

                            //line_size = getline(&line_buf, &line_buf_size, fptr_tmp);
                            FILE *fptr_usr;
                            fptr_usr = (fopen("usr.txt", "w"));
                            if (fptr_usr == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }

                            FILE *fptr_tmp;
                            fptr_tmp = (fopen("tmp2.txt", "r"));
                            if (fptr_tmp == NULL)
                            {
                                printf("Error!");
                                exit(1);
                            }
                            char *line_buf = NULL;
                            size_t line_buf_size = 0;
                            ssize_t line_size;
                            int cnt = 0;
                            for (int j = 1; j <= number_total; j++)
                            {

                                line_size = getline(&line_buf, &line_buf_size, fptr_tmp);
                                char line_buf_copy[MAXDATASIZE];
                                strncpy(line_buf_copy, line_buf, sizeof(line_buf_copy));

                                char *token = strtok(line_buf, " ");
                                int seq = atoi(token);

                                bool isSender = false;
                                bool isReceiver = false;
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) == 0)
                                {
                                    isSender = true;
                                }

                                //printf(" %s\n", token); //printing each token

                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) == 0)
                                {
                                    isReceiver = true;
                                }

                                //printf(" %s\n", token); //printing each token
                                if (isSender || isReceiver)
                                {
                                    fprintf(fptr_usr, "%s", line_buf_copy);
                                    cnt++;
                                }
                            }
                            fclose(fptr_tmp);

                            fclose(fptr_usr);
                            remove("tmp2.txt");

                            FILE *fptr_usr1 = (fopen("usr.txt", "r"));
                            line_buf = NULL;
                            line_buf_size = 0;
                            FILE *fptr_usrname = (fopen("usrname.txt", "w"));
                            char **str = (char **)malloc(sizeof(char *) * cnt);
                            for (int i = 0; i < cnt; i++)
                            {
                                str[i] = (char *)malloc(sizeof(char) * MAXDATASIZE);
                            }
                            int usrcnt = 0;
                            for (int j = 0; j < cnt; j++)
                            {
                                line_size = getline(&line_buf, &line_buf_size, fptr_usr1);
                                char line_buf_copy[MAXDATASIZE];
                                strncpy(line_buf_copy, line_buf, sizeof(line_buf_copy));
                                //printf("%s",line_buf);
                                char *token = strtok(line_buf_copy, " ");
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {
                                    bool usrexist = false;
                                    for (int i = 0; i < cnt; i++)
                                    {
                                        if (strcmp(token, str[i]) == 0)
                                            usrexist = true;
                                    }
                                    if (!usrexist)
                                    {
                                        strcpy(str[j], token);
                                        //printf("%s\n",str[j]);
                                        fprintf(fptr_usrname,"%s\n",str[j]);
                                        usrcnt++;
                                    }
                                }
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {
                                    bool usrexist = false;
                                    for (int i = 0; i < cnt; i++)
                                    {
                                        if (strcmp(token, str[i]) == 0)
                                            usrexist = true;
                                    }
                                    if (!usrexist)
                                    {
                                        strcpy(str[j], token);
                                        //printf("%s\n",str[j]);
                                        fprintf(fptr_usrname,"%s\n",str[j]);
                                        usrcnt++;
                                    }
                                }
                            }
                            
                            fclose(fptr_usr1);
                            fclose(fptr_usrname);
                            for (int i = 0; i < cnt; i++)
                            {
                                free(str[i]);
                            }
                            free(str);

                            char **usrname = (char **)malloc(sizeof(char *) * usrcnt);
                            for (int i = 0; i < usrcnt; i++)
                            {
                                usrname[i] = (char *)malloc(sizeof(char) * MAXDATASIZE);
                            }

                            char **str2 = (char **)malloc(sizeof(char *) * cnt);
                            for (int i = 0; i < cnt; i++)
                            {
                                str2[i] = (char *)malloc(sizeof(char) * MAXDATASIZE);
                            }
                            FILE *fptr_usr2 = (fopen("usr.txt", "r"));
                            line_buf = NULL;
                            line_buf_size = 0;
                            int usrindex = 0;
                            for (int j = 0; j < cnt; j++)
                            {
                                line_size = getline(&line_buf, &line_buf_size, fptr_usr2);
                                char line_buf_copy[MAXDATASIZE];
                                strncpy(line_buf_copy, line_buf, sizeof(line_buf_copy));

                                char *token = strtok(line_buf_copy, " ");
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {
                                    bool usrexist = false;
                                    for (int i = 0; i < cnt; i++)
                                    {
                                        if (strcmp(token, str2[i]) == 0)
                                            usrexist = true;
                                    }
                                    if (!usrexist)
                                    {
                                        strcpy(str2[j], token);
                                        strcpy(usrname[usrindex], token);
                                        usrindex++;
                                    }
                                }
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {
                                    bool usrexist = false;
                                    for (int i = 0; i < cnt; i++)
                                    {
                                        if (strcmp(token, str2[i]) == 0)
                                            usrexist = true;
                                    }
                                    if (!usrexist)
                                    {
                                        strcpy(str2[j], token);
                                        strcpy(usrname[usrindex], token);
                                        usrindex++;
                                    }
                                }
                            }
                            fclose(fptr_usr2);
                            
                            /*for (int i = 0; i < usrcnt; i++)
                            {
                                printf("%s ", usrname[i]);
                            }
                            printf("\n");*/
                            FILE *fptr_usr3 = (fopen("usr.txt", "r"));
                            line_buf = NULL;
                            line_buf_size = 0;
                            int *cntTrans = (int *)calloc(usrcnt, sizeof(int));
                            for (int i = 0; i < usrcnt; i++)
                            {
                                cntTrans[i] = 0;
                            }
                            int *amtTrans = (int *)calloc(usrcnt, sizeof(int));
                            for (int i = 0; i < usrcnt; i++)
                            {
                                amtTrans[i] = 0;
                            }
                            for (int j = 0; j < cnt; j++)
                            {
                                line_size = getline(&line_buf, &line_buf_size, fptr_usr3);
                                char line_buf_copy[MAXDATASIZE];
                                strncpy(line_buf_copy, line_buf, sizeof(line_buf_copy));

                                bool isSender = false;
                                bool isReceiver = false;
                                char *token = strtok(line_buf_copy, " ");
                                int index;
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {

                                    for (int i = 0; i < usrcnt; i++)
                                    {
                                        if (strcmp(token, usrname[i]) == 0)
                                            index = i;
                                    }
                                    cntTrans[index]++;
                                    isSender = true;
                                }
                                token = strtok(NULL, " ");
                                if (strcmp(token, sender) != 0)
                                {

                                    for (int i = 0; i < usrcnt; i++)
                                    {
                                        if (strcmp(token, usrname[i]) == 0)
                                            index = i;
                                    }
                                    cntTrans[index]++;
                                    isReceiver = true;
                                }
                                token = strtok(NULL, " ");
                                int curAmt = atoi(token);
                                if (isSender)
                                    amtTrans[index] -= curAmt;
                                else if (isReceiver)
                                    amtTrans[index] += curAmt;
                            }
                            fclose(fptr_usr3);
                           
                            
                            //bubble sort
                            int tmp_cnt,tmp_amt;
                            char *tmp_name = (char *)malloc(sizeof(char) * MAXDATASIZE);
                            for (int i = 0; i < usrcnt - 1; i++){
                                for (int j = 0; j < usrcnt - 1 - i; j++){
                                    if (cntTrans[j] < cntTrans[j + 1])
                                    {
                                        tmp_cnt = cntTrans[j];
                                        cntTrans[j] = cntTrans[j + 1];
                                        cntTrans[j + 1] = tmp_cnt;
                                        tmp_amt = amtTrans[j];
                                        amtTrans[j] = amtTrans[j + 1];
                                        amtTrans[j + 1] = tmp_amt;
                                        strcpy(tmp_name,usrname[j]);
                                        strcpy(usrname[j],usrname[j+1]);
                                        strcpy(usrname[j+1],tmp_name);

                                    }
                                }
                                    
                            }
                            
                            if (send(newfd, &usrcnt, sizeof(int), 0) == -1)
                                perror("send");

                            for (int i=0;i<usrcnt;i++){
                                if (send(newfd, usrname[i], MAXDATASIZE-1, 0) == -1)
                                    perror("send");
                                if (send(newfd, cntTrans+i, sizeof(int), 0) == -1)
                                    perror("send");
                                if (send(newfd, amtTrans+i, sizeof(int), 0) == -1)
                                    perror("send");
                            }
                            remove("usr.txt");
                            remove("usrname.txt");

                                
                            

                            /*char *lbc;
                                lbc = (char*)malloc(sizeof(char)*MAXDATASIZE);  
                                strcpy(lbc, line_buf);
                                printf("%s",lbc); 
                                free(lbc);*/
                        }
                    }
                }
                // END handle data from client
            } // END got ready-to-read from poll()
        }     // END looping through file descriptors
    }         // END for(;;)--and you thought it would never end!

    return 0;
}
