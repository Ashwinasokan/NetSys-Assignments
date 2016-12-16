#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <dirent.h>
/* You will have to modify the program below */

#define MAXBUFSIZE 1000
#define MAXMESSAGESIZE 2000

int sendMyPacketS(int s, const void *buf, size_t len,int flags, const struct sockaddr *remote,int remote_length, struct sockaddr *restrict address,socklen_t *restrict address_len);
int recvMyPacketS(int socket, void *restrict buffer, size_t length,int flags, struct sockaddr *restrict address,socklen_t *restrict address_len, const struct sockaddr *remote,int remote_length);
int totalReadS = 0;
int totalWrittenS = 0;
int totalPacketsSentS = 0;
int totalPacketsReceivedS = 0;
const char *ACK = "ACK";

int main (int argc, char * argv[] )
{
	int sock;                           //This will be our socket
	struct sockaddr_in sin, remote;     //"Internet socket address structure"
	unsigned int remote_length;         //length of the sockaddr_in structure
	int nbytes;                        //number of bytes we receive in our message
	char buffer[MAXBUFSIZE];             //a buffer to store our received message
	char command[MAXBUFSIZE];
	char message[MAXBUFSIZE];
	char left[MAXBUFSIZE];
	char right[MAXBUFSIZE];
	char *l,*r;
	DIR *dp;
	struct dirent *ep;
	const char *START = "START";
	const char *STOP = "STOP";
	const char *ERROR = "ERROR";
	const char *NUL = "NULL";
	const char *GET = "get";
	const char *PUT = "put";
	const char *LS = "ls";
	const char *EXIT = "exit";
	FILE *fp;
	int last_pos = 0;
    	int curr_pos = 0;
	int nWritten;
	if (argc != 2)
	{
		printf ("USAGE:  <port>\n");
		exit(1);
	}

	/******************
	  This code populates the sockaddr_in struct with
	  the information about our socket
	 ******************/
	bzero(&sin,sizeof(sin));                    //zero the struct
	sin.sin_family = AF_INET;                   //address family
	sin.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
	sin.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine


	//Causes the system to create a generic socket of type UDP (datagram)
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("unable to create socket");
	}

	printf("socket created\n");
	/******************
	  Once we've created a socket, we must bind that socket to the 
	  local address and port we've supplied in the sockaddr_in struct
	 ******************/
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		printf("unable to bind socket\n");
	}
	printf("socket binded\n");
	remote_length = sizeof(remote);
	while (1) 
	{
		last_pos = 0;
    		curr_pos = 0;
		totalReadS = 0;
		totalWrittenS = 0;
		totalPacketsSentS = 0;
		totalPacketsReceivedS = 0;
		bzero(buffer,sizeof(buffer));
		nbytes = recvMyPacketS(sock, buffer, MAXBUFSIZE, 0, (struct sockaddr *) &remote, &remote_length, (struct sockaddr *) &remote, remote_length);
		printf("Command %s\n", buffer);
		strcpy(command, buffer);
		l = strtok(command, " ");
		r = strtok(NULL, " ");
		strcpy(left, l);
		if (r != NULL) 
		{
			strcpy(right, r);
		}
		else 
		{
			strcpy(right, NUL);
		}	
		strcpy(command, buffer);
		if (strcmp(right, NUL) == 0 && strcmp(left, LS) == 0) 
		{
			dp = opendir ("./");
			if (dp != NULL)
			{
     				while (ep = readdir (dp))
      				{
					nbytes = sendMyPacketS(sock, ep->d_name, strlen(ep->d_name), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
				}
      				(void) closedir (dp);
				nbytes = sendMyPacketS(sock, STOP, strlen(STOP), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
    			}
  			else
			{
				nbytes = sendMyPacketS(sock, ERROR, strlen(ERROR), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
				strcpy(message, "Couldn't open the directory");
				nbytes = sendMyPacketS(sock, message, strlen(message), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
				nbytes = sendMyPacketS(sock, STOP, strlen(STOP), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
			}
		}
		else if (strcmp(right, NUL) == 0 && strcmp(left, EXIT) == 0) 
		{
			break;
		}
		else if (strcmp(left, GET) == 0)
		{
			/* open the file to be sent */
    			fp = fopen(right, "rb");
    			if (fp == NULL) 
			{
				nbytes = sendMyPacketS(sock, ERROR, strlen(ERROR), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
				strcpy(message, "File not found\n");
      				nbytes = sendMyPacketS(sock, message, strlen(message), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
				nbytes = sendMyPacketS(sock, STOP, strlen(STOP), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
    			}
			printf("Found file success\n");
			bzero(buffer,sizeof(buffer));
			while (fgets(buffer,MAXBUFSIZE,fp) != NULL) // expect 1 successful conversion
			{
				curr_pos = ftell(fp);
  				nbytes = sendMyPacketS(sock, buffer, curr_pos-last_pos, 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
				totalReadS += curr_pos-last_pos;
				totalWrittenS += nbytes;
				last_pos = curr_pos;
				bzero(buffer,sizeof(buffer));
			}
			if (feof(fp)) 
			{
  				// hit end of file
				nbytes = sendMyPacketS(sock, STOP, strlen(STOP), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
			}
			else
			{
  				// some other error interrupted the read
				nbytes = sendMyPacketS(sock, ERROR, strlen(ERROR), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
				strcpy(message, "File read error");
      				nbytes = sendMyPacketS(sock, message, strlen(message), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
				nbytes = sendMyPacketS(sock, STOP, strlen(STOP), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
			}
			printf("File closed %d totalRead %d totalWritten %d\n",fclose(fp),totalReadS,totalWrittenS);
		}
		else if (strcmp(left, PUT) == 0)
		{
			/* write the file to be saved */
    			fp = fopen(right, "wb");
			if (fp == NULL) 
			{
				nbytes = sendMyPacketS(sock, ERROR, strlen(ERROR), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
				strcpy(message, "File write error");
      				nbytes = sendMyPacketS(sock, message, strlen(message), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
				nbytes = sendMyPacketS(sock, STOP, strlen(STOP), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
    			}
			while (1) // expect 1 successful conversion
			{
				bzero(buffer,sizeof(buffer));
				nbytes = recvMyPacketS(sock, buffer, MAXBUFSIZE, 0, (struct sockaddr *) &remote, &remote_length, (struct sockaddr *) &remote, remote_length);
				if (strncmp(buffer, STOP,nbytes) == 0)
				{
					break;
				}
				nWritten = fwrite(buffer,1,nbytes,fp);
				totalReadS += nbytes;
				totalWrittenS += nWritten;
			}
			printf("File closed %d totalRead %d totalWritten %d\n",fclose(fp),totalReadS,totalWrittenS);
		}
		else 
		{
			nbytes = sendMyPacketS(sock, command, strlen(command), 0, (struct sockaddr *) &remote, remote_length, (struct sockaddr *) &remote, &remote_length);
		}
		printf("Request Complete totalPacketsSent %d totalPacketsReceived %d\n",totalPacketsSentS,totalPacketsReceivedS);
	}
	close(sock);
}

int sendMyPacketS(int sock, const void *buf, size_t len,int flags,const struct sockaddr *remote,int remote_length, struct sockaddr *restrict address,socklen_t *restrict address_len)
{
	char ack[MAXBUFSIZE];
	int bytesSent = 0;
	int bytesReceived = 0;
	do {
      		bytesSent = 0;
		bzero(ack,sizeof(ack));
		bytesSent = sendto(sock, buf, len, flags, remote, remote_length);
		totalPacketsSentS += 1;
		bytesReceived = recvfrom(sock, ack, MAXBUFSIZE, flags, address, address_len);
		totalPacketsReceivedS += 1;
	}while( strcmp(ack, ACK) != 0 );
	return bytesSent;
}

int recvMyPacketS(int sock, void *restrict buffer, size_t length,int flags, struct sockaddr *restrict address,socklen_t *restrict address_len, const struct sockaddr *remote,int remote_length)
{
	bzero(buffer,sizeof(buffer));
	int bytesReceived = recvfrom(sock, buffer, MAXBUFSIZE, flags, address, address_len);
	totalPacketsReceivedS += 1;
	int bytesSent = sendto(sock, ACK, strlen(ACK), flags, remote, remote_length);
	totalPacketsSentS += 1;
	return bytesReceived;
}
