/*
** listener.c -- a datagram sockets "server" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>

#define MYPORT "23483" // the port users will be connecting to
#define UDP_PORT "24483"
#define LOCALHOST "127.0.0.1"
#define MAXDATASIZE 512
#define INITIAL_BALANCE 1000

// get sockaddr, IPv4 or IPv6:
//codes from Beej's
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

//codes edited from Beej's
int main(void)
{

	// make sure the txt file is in good format
	FILE *fp1;
	fp1 = fopen("block3.txt", "r");
	FILE *fp1_tmp;
	;
	fp1_tmp = fopen("block3_tmp.txt", "w");
	char *line_buf = NULL;
	size_t line_buf_size = 0;
	ssize_t line_size;
	line_size = getline(&line_buf, &line_buf_size, fp1);
	int number_of_lines = 0;
	while (line_size >= 2)
	{

		char line_buf_copy[MAXDATASIZE];
		strncpy(line_buf_copy, line_buf, sizeof(line_buf_copy));
		int len = strlen(line_buf_copy);
		//printf("contents: %s", line_buf_copy);
		//printf("strlen: %d\n", len);
		//printf("strlen0: %c\n", line_buf_copy[len-1]);
		if (line_buf_copy[len - 1] != '\n')
		{
			line_buf_copy[len] = '\n';
		}
		//printf("contents: %s", line_buf_copy);
		fprintf(fp1_tmp, "%s", line_buf_copy);
		line_size = getline(&line_buf, &line_buf_size, fp1);
		number_of_lines++;
	}

	fclose(fp1);
	fclose(fp1_tmp);
	remove("block3.txt");
    rename("block3_tmp.txt", "block3.txt");

	//Set up socket
	//codes edited from Beej's codes
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	char sender[MAXDATASIZE];
	char receiver[MAXDATASIZE];
	socklen_t addr_len;
	//char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo("localhost", MYPORT, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			perror("listener: socket");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("listener: bind");
			continue;
		}

		break;
	}

	if (p == NULL)
	{
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	printf("The ServerC is up and running using UDP on port %s\n", MYPORT);

	while (1)
	{
		addr_len = sizeof their_addr;

		int operation_status = 0;
		if ((numbytes = recvfrom(sockfd, &operation_status, sizeof(int), 0,
								 (struct sockaddr *)&their_addr, &addr_len)) == -1)
		{
			perror("recvfrom");
			exit(1);
		}
		//printf("operation_status=%d\n", operation_status);
		//***CHECK BALANCE
		if (operation_status == 1)
		{
			if ((numbytes = recvfrom(sockfd, sender, MAXDATASIZE - 1, 0,
									 (struct sockaddr *)&their_addr, &addr_len)) == -1)
			{
				perror("recvfrom");
				exit(1);
			}
			sender[numbytes] = '\0';
			printf("The ServerC received a request from the Main Server\n");
			//printf("Get sender: %s\n", sender); //*******

			FILE *fp1;
			fp1 = fopen("block3.txt", "r");
			char *line_buf = NULL;
			size_t line_buf_size = 0;
			ssize_t line_size;
			line_size = getline(&line_buf, &line_buf_size, fp1);
			int number_of_lines = 0;
			bool isInNetwork = false;
			int transOut = 0;
			int transIn = 0;
			bool isOut = false;
			bool isIn = false;
			//iterate log file
			while (line_size >= 2)
			{
				//printf("contents: %s", line_buf);

				char *token = strtok(line_buf, " ");
				//int seq = atoi(token);
				//printf(" %d\n", seq); //printing each token

				token = strtok(NULL, " ");
				if (strcmp(token, sender) == 0)
				{
					isInNetwork = true;
					isOut = true;
				}
				//printf(" %s\n", token); //printing each token

				token = strtok(NULL, " ");
				if (strcmp(token, sender) == 0)
				{
					isInNetwork = true;
					isIn = true;
				}
				//printf(" %s\n", token); //printing each token

				token = strtok(NULL, " ");
				int amt = atoi(token);
				//printf(" %d\n", amt); //printing each token
				if (isOut)
				{
					transOut += amt;
				}
				else if (isIn)
				{
					transIn += amt;
				}
				line_size = getline(&line_buf, &line_buf_size, fp1);
				number_of_lines++;
				isOut = false;
				isIn = false;
			}

			fclose(fp1);
			int balance_changeA = 0;
			if (isInNetwork)
			{
				balance_changeA = 0 - transOut + transIn;
			}
			else
			{
				balance_changeA = 0;
			}
			if ((numbytes = sendto(sockfd, &isInNetwork, sizeof(bool), 0,
								   (struct sockaddr *)&their_addr, addr_len)) == -1)
			{
				perror("talker: sendto 1");
				exit(1);
			}
			if ((numbytes = sendto(sockfd, &balance_changeA, sizeof(int), 0,
								   (struct sockaddr *)&their_addr, addr_len)) == -1)
			{
				perror("talker: sendto 1");
				exit(1);
			}
			printf("The ServerC finished sending the response to the Main Server\n");
		}

		//***TXCOINS
		else if (operation_status == 2)
		{
			if ((numbytes = recvfrom(sockfd, sender, MAXDATASIZE - 1, 0,
									 (struct sockaddr *)&their_addr, &addr_len)) == -1)
			{
				perror("recvfrom");
				exit(1);
			}
			sender[numbytes] = '\0';
			if ((numbytes = recvfrom(sockfd, receiver, MAXDATASIZE - 1, 0,
									 (struct sockaddr *)&their_addr, &addr_len)) == -1)
			{
				perror("recvfrom");
				exit(1);
			}
			receiver[numbytes] = '\0';
			printf("The ServerC received a request from the Main Server\n");
			FILE *fp1;
			fp1 = fopen("block3.txt", "r");
			char *line_buf = NULL;
			size_t line_buf_size = 0;
			ssize_t line_size;
			line_size = getline(&line_buf, &line_buf_size, fp1);
			int number_of_lines = 0;
			bool isInNetwork = false;
			bool isInNetwork_rec = false;
			int transOut = 0;
			int transIn = 0;
			bool isOut = false;
			bool isIn = false;
			int maxSeq = 0;
			//iterate log file
			while (line_size >= 2)
			{
				//printf("contents: %s", line_buf);

				char *token = strtok(line_buf, " ");
				int seq = atoi(token);
				if (seq > maxSeq)
				{
					maxSeq = seq;
				}
				//printf(" %d\n", seq); //printing each token

				token = strtok(NULL, " ");
				if (strcmp(token, sender) == 0)
				{
					isInNetwork = true;
					isOut = true;
				}
				if (strcmp(token, receiver) == 0)
				{
					isInNetwork_rec = true;
				}
				//printf(" %s\n", token); //printing each token

				token = strtok(NULL, " ");
				if (strcmp(token, sender) == 0)
				{
					isInNetwork = true;
					isIn = true;
				}
				if (strcmp(token, receiver) == 0)
				{
					isInNetwork_rec = true;
				}
				//printf(" %s\n", token); //printing each token

				token = strtok(NULL, " ");
				int amt = atoi(token);
				//printf(" %d\n", amt); //printing each token
				if (isOut)
				{
					transOut += amt;
				}
				else if (isIn)
				{
					transIn += amt;
				}
				line_size = getline(&line_buf, &line_buf_size, fp1);
				number_of_lines++;
				isOut = false;
				isIn = false;
			}

			fclose(fp1);

			int balance_changeA = 0;
			if (isInNetwork)
			{
				balance_changeA = 0 - transOut + transIn;
			}
			else
			{
				balance_changeA = 0;
			}
			if ((numbytes = sendto(sockfd, &isInNetwork, sizeof(bool), 0,
								   (struct sockaddr *)&their_addr, addr_len)) == -1)
			{
				perror("talker: sendto 1");
				exit(1);
			}
			if ((numbytes = sendto(sockfd, &isInNetwork_rec, sizeof(bool), 0,
								   (struct sockaddr *)&their_addr, addr_len)) == -1)
			{
				perror("talker: sendto 1");
				exit(1);
			}
			if ((numbytes = sendto(sockfd, &balance_changeA, sizeof(int), 0,
								   (struct sockaddr *)&their_addr, addr_len)) == -1)
			{
				perror("talker: sendto 1");
				exit(1);
			}
			if ((numbytes = sendto(sockfd, &maxSeq, sizeof(int), 0,
								   (struct sockaddr *)&their_addr, addr_len)) == -1)
			{
				perror("talker: sendto 1");
				exit(1);
			}
			printf("The ServerC finished sending the response to the Main Server\n");
		}

		//***write logfile
		else if (operation_status == 3)
		{

			printf("The ServerC received a request from the Main Server\n");
			int newSeq = 0;
			int transamount = 0;

			if ((numbytes = recvfrom(sockfd, &newSeq, sizeof(int) - 1, 0,
									 (struct sockaddr *)&their_addr, &addr_len)) == -1)
			{
				perror("recvfrom");
				exit(1);
			}
			if ((numbytes = recvfrom(sockfd, sender, MAXDATASIZE - 1, 0,
									 (struct sockaddr *)&their_addr, &addr_len)) == -1)
			{
				perror("recvfrom");
				exit(1);
			}
			sender[numbytes] = '\0';
			if ((numbytes = recvfrom(sockfd, receiver, MAXDATASIZE - 1, 0,
									 (struct sockaddr *)&their_addr, &addr_len)) == -1)
			{
				perror("recvfrom");
				exit(1);
			}
			receiver[numbytes] = '\0';
			if ((numbytes = recvfrom(sockfd, &transamount, sizeof(int) - 1, 0,
									 (struct sockaddr *)&their_addr, &addr_len)) == -1)
			{
				perror("recvfrom");
				exit(1);
			}

			FILE *fptr;
			fptr = (fopen("block3.txt", "a"));
			if (fptr == NULL)
			{
				printf("Error!");
				exit(1);
			}

			fprintf(fptr, "%d %s %s %d\n", newSeq, sender, receiver, transamount);
			printf("The ServerC finished sending the response to the Main Server\n");
			fclose(fptr);
		}

		//***Get TXLIST
		else if (operation_status == 4)
		{
			FILE *fp1;
			fp1 = fopen("block3.txt", "r");
			char *line_buf = NULL;
			size_t line_buf_size = 0;
			ssize_t line_size;
			line_size = getline(&line_buf, &line_buf_size, fp1);
			int number_of_lines = 0;
			while (line_size >= 2)
			{

				line_size = getline(&line_buf, &line_buf_size, fp1);
				number_of_lines++;
			}
			if ((numbytes = sendto(sockfd, &number_of_lines, sizeof(int), 0,
								   (struct sockaddr *)&their_addr, addr_len)) == -1)
			{
				perror("talker: sendto 1");
				exit(1);
			}
			//printf("number of lines: %d\n",number_of_lines);
			fclose(fp1);
			FILE *fp2;
			fp2 = fopen("block3.txt", "r");
			for (int i = 0; i < number_of_lines; i++)
			{
				line_size = getline(&line_buf, &line_buf_size, fp2);
				if ((numbytes = sendto(sockfd, line_buf, MAXDATASIZE - 1, 0,
									   (struct sockaddr *)&their_addr, addr_len)) == -1)
				{
					perror("talker: sendto 1");
					exit(1);
				}
			}
		}

		//***Get STATS
		else if (operation_status == 5)
		{
			FILE *fp1;
			fp1 = fopen("block3.txt", "r");
			char *line_buf = NULL;
			size_t line_buf_size = 0;
			ssize_t line_size;
			line_size = getline(&line_buf, &line_buf_size, fp1);
			int number_of_lines = 0;
			while (line_size >= 2)
			{

				line_size = getline(&line_buf, &line_buf_size, fp1);
				number_of_lines++;
			}
			if ((numbytes = sendto(sockfd, &number_of_lines, sizeof(int), 0,
								   (struct sockaddr *)&their_addr, addr_len)) == -1)
			{
				perror("talker: sendto 1");
				exit(1);
			}
			//printf("number of lines: %d\n",number_of_lines);
			fclose(fp1);
			FILE *fp2;
			fp2 = fopen("block3.txt", "r");
			for (int i = 0; i < number_of_lines; i++)
			{
				line_size = getline(&line_buf, &line_buf_size, fp2);
				if ((numbytes = sendto(sockfd, line_buf, MAXDATASIZE - 1, 0,
									   (struct sockaddr *)&their_addr, addr_len)) == -1)
				{
					perror("talker: sendto 1");
					exit(1);
				}
			}
		}
	}

	close(sockfd);

	return 0;
}
