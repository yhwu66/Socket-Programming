/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdbool.h>

#include <arpa/inet.h>

#define PORT "25483" // the port client will be connecting to
#define LOCALHOST "127.0.0.1"
#define MAXDATASIZE 512 // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6: from Beej's code
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{

	//The codes below are edited from Beej's codes
	int sockfd, numbytes;
	//char buf[MAXDATASIZE];
	//char sender[MAXDATASIZE];
	//char receiver[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	//char s[INET6_ADDRSTRLEN];
	int operation = 0; //1 for checking ballance; 2 for transaction

	if (argc == 2)
	{
		if (strcmp(argv[1], "TXLIST") != 0)
		{
			operation = 1;
		}
		else
		{
			operation = 4;
		}
	}
	else if (argc == 4)
	{
		operation = 2;
	}
	else if (argc == 3)
	{
		operation = 5;
	}
	//printf("operation: %d\n",operation);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(LOCALHOST, PORT, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			perror("client: connect");
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL)
	{
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	/*inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			  s, sizeof s);
	printf("client: connecting to %s\n", s);*/

	freeaddrinfo(servinfo); // all done with this structure
	printf("The clientA is up and runing\n");
	if (operation == 1)
	{
		int operlen = strlen(argv[1]);
		if (send(sockfd, &operation, sizeof(int), 0) == -1)
			perror("send");
		if (send(sockfd, argv[1], operlen, 0) == -1)
			perror("send");
		printf("%s sent a balance enquiry request to the main server\n", argv[1]);
		int balance;
		if ((numbytes = recv(sockfd, &balance, sizeof(int), 0)) == -1)
		{
			perror("recv");
			exit(1);
		}

		if (balance == -1)
		{
			printf("\"%s\" is not part of the network\n", argv[1]);
		}
		else
		{
			printf("The current balance of \"%s\" is: %d alicoins\n", argv[1], balance);
		}

		close(sockfd);
	}
	else if (operation == 2)
	{

		/*int senderlen = strlen(argv[1]);
		int receiverlen = strlen(argv[2]);
		int amountlen = strlen(argv[3]);*/
		if (send(sockfd, &operation, sizeof(int), 0) == -1)
			perror("send");
		if (send(sockfd, argv[1], MAXDATASIZE - 1, 0) == -1)
			perror("send");
		if (send(sockfd, argv[2], MAXDATASIZE - 1, 0) == -1)
			perror("send");
		if (send(sockfd, argv[3], MAXDATASIZE - 1, 0) == -1)
			perror("send");
		printf("%s has requested to transfer %s coins to %s\n", argv[1], argv[3], argv[2]);
		int status = 1; //1 for seccess,2 for insufficent balance, 3 for sender out network, 4 for receiver out network,5 for 2 out network
		if ((numbytes = recv(sockfd, &status, sizeof(int), 0)) == -1)
		{
			perror("recv");
			exit(1);
		}
		int balance = 0;
		if ((numbytes = recv(sockfd, &balance, sizeof(int), 0)) == -1)
		{
			perror("recv");
			exit(1);
		}

		if (status == 1)
		{
			printf("%s successfully transfered %s alcoins to %s\n", argv[1], argv[3], argv[2]);
			printf("The current balance of \"%s\" is: %d alicoins\n", argv[1], balance);
		}
		else if (status == 2)
		{
			printf("%s was unable to transfer %s alcoins to %s because of insufficient balance\n", argv[1], argv[3], argv[2]);
			printf("The current balance of \"%s\" is: %d alicoins\n", argv[1], balance);
		}
		else if (status == 3)
		{
			printf("Unable to proceed with transaction as %s is not part of the network\n", argv[1]);
		}
		else if (status == 4)
		{
			printf("Unable to proceed with transaction as %s is not part of the network\n", argv[2]);
		}
		else if (status == 5)
		{
			printf("Unable to proceed with transaction as %s and %s are not part of the network\n", argv[1], argv[2]);
		}

		close(sockfd);
	}
	else if (operation == 4)
	{
		if (send(sockfd, &operation, sizeof(int), 0) == -1)
			perror("send");
		printf("clientA sent a sorted list request to the main server\n");
	}
	else if (operation == 5)
	{
		if (send(sockfd, &operation, sizeof(int), 0) == -1)
			perror("send");
		if (send(sockfd, argv[1], MAXDATASIZE - 1, 0) == -1)
			perror("send");
		printf("%s sent a statistics enquiry request to the main server\n", argv[1]);

		int usrcnt = 0;
		if ((numbytes = recv(sockfd, &usrcnt, sizeof(int), 0)) == -1)
		{
			perror("recv");
			exit(1);
		}
		if (usrcnt > 0)
		{
			printf("%s statistics enquiry are the following.:\n", argv[1]);
			printf("Rank--Username--NumofTransactions--Total\n");
			for (int i = 1; i <= usrcnt; i++)
			{
				char name[MAXDATASIZE];
				int cntTrans = 0;
				int amtTrans = 0;
				if ((numbytes = recv(sockfd, name, MAXDATASIZE - 1, 0)) == -1)
				{
					perror("recv");
					exit(1);
				}
				if ((numbytes = recv(sockfd, &cntTrans, sizeof(int), 0)) == -1)
				{
					perror("recv");
					exit(1);
				}
				if ((numbytes = recv(sockfd, &amtTrans, sizeof(int), 0)) == -1)
				{
					perror("recv");
					exit(1);
				}
				printf("%-4d  %-12s      %-12d%-12d\n", i, name, cntTrans, amtTrans);
			}
		}
		else
		{
			printf("%s is not part of the network\n", argv[1]);
		}
	}

	return 0;
}
