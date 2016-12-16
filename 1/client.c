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
#include <errno.h>

#define MAXBUFSIZE 1000
#define MAXMESSAGESIZE 2000

/* You will have to modify the program below */

int sendMyPacketC(int s, const void *buf, size_t len,int flags, const struct sockaddr *remote,int remote_length, struct sockaddr *restrict address,socklen_t *restrict address_len);
int recvMyPacketC(int socket, void *restrict buffer, size_t length,int flags, struct sockaddr *restrict address,socklen_t *restrict address_len, const struct sockaddr *remote,int remote_length);

int totalReadC = 0;
int totalWrittenC = 0;
int totalPacketsSentC = 0;
int totalPacketsReceivedC = 0;
const char *ACKC = "ACK";

int main (int argc, char * argv[])
{
	int nbytes;                             // number of bytes send by sendto()
	int sock;                               //this will be our socket
	char buffer[MAXBUFSIZE];
	char command[MAXBUFSIZE];
	int remote_length;
	struct sockaddr_in remote;              //"Internet socket address structure"
	char message[MAXBUFSIZE];
	char left[MAXBUFSIZE];
	char right[MAXBUFSIZE];
	char *l,*r;
	const char *START = "START";
	const char *STOP = "STOP";
	const char *ERROR = "ERROR";
	const char *NUL = "NULL";
	const char *GET = "get";
	const char *PUT = "put";
	const char *LS = "ls";
	const char *EXIT = "exit";
	FILE *fp;
	int nWritten;
	int last_pos = 0;
    	int curr_pos = 0;
	if (argc < 3)
	{
		printf("USAGE:  <server_ip> <server_port>\n");
		exit(1);
	}

	/******************
	  Here we populate a sockaddr_in struct with
	  information regarding where we'd like to send our packet 
	  i.e the Server.
	 ******************/
	bzero(&remote,sizeof(remote));               //zero the struct
	remote.sin_family = AF_INET;                 //address family
	remote.sin_port = htons(atoi(argv[2]));      //sets port to network byte order
	remote.sin_addr.s_addr = inet_addr(argv[1]); //sets remote IP address

	//Causes the system to create a generic socket of type UDP (datagram)
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("unable to create socket");
	}
	printf("socket created\n");
	/******************
	  sendto() sends immediately.  
	  it will report an error if the message fails to leave the computer
	  however, with UDP, there is no error if the message is lost in the network once it leaves the computer.
	 ******************/
	while (1) 
	{
		last_pos = 0;
    		curr_pos = 0;
		totalReadC = 0;
		totalWrittenC = 0;
		totalPacketsSentC = 0;
		totalPacketsReceivedC = 0;
		bzero(buffer,sizeof(buffer));
		printf("Enter command:");
		fgets (buffer, MAXBUFSIZE, stdin);
		if ((strlen(buffer)>0) && (buffer[strlen (buffer) - 1] == '\n'))
     			buffer[strlen (buffer) - 1] = '\0';
		remote_length = sizeof(remote);
		strcpy(command, buffer);
		l = strtok(buffer, " ");
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
		if (strcmp(right, NUL) == 0 && strcmp(left, LS) == 0) 
		{
			nbytes = sendMyPacketC(sock, command, strlen(command), 0, (struct sockaddr *) &remote, remote_length,(struct sockaddr *) &remote, &remote_length);
			while (1)
			{
				bzero(buffer,sizeof(buffer));
				nbytes = recvMyPacketC(sock, buffer, MAXBUFSIZE, 0, (struct sockaddr *) &remote, &remote_length, (struct sockaddr *) &remote, remote_length);
				if (strcmp(buffer, STOP) == 0) 
				{
					break;
				}
				else if (strcmp(buffer, ERROR) == 0)
				{
					bzero(buffer,sizeof(buffer));
					nbytes = recvMyPacketC(sock, buffer, MAXBUFSIZE, 0, (struct sockaddr *) &remote, &remote_length, (struct sockaddr *) &remote, remote_length);
					printf("Error: %s\n",buffer);
					break;
				}
				else 
				{
					printf("%s\n",buffer);
				}
			}
		}
		else if (strcmp(right, NUL) == 0 && strcmp(left, EXIT) == 0) 
		{
			nbytes = sendMyPacketC(sock, command, strlen(command), 0, (struct sockaddr *) &remote, remote_length,(struct sockaddr *) &remote, &remote_length);
			break;
		}
		else if (strcmp(left, GET) == 0)
		{
			nbytes = sendMyPacketC(sock, command, strlen(command), 0, (struct sockaddr *) &remote, remote_length,(struct sockaddr *) &remote, &remote_length);
			bzero(buffer,sizeof(buffer));
			nbytes = recvMyPacketC(sock, buffer, MAXBUFSIZE, 0, (struct sockaddr *) &remote, &remote_length, (struct sockaddr *) &remote, remote_length);
			if (strncmp(buffer, ERROR,nbytes) == 0)
			{
				bzero(buffer,sizeof(buffer));
				nbytes = recvMyPacketC(sock, buffer, MAXBUFSIZE, 0, (struct sockaddr *) &remote, &remote_length, (struct sockaddr *) &remote, remote_length);
				printf("Error: %s\n",buffer);
				break;
			}
			/* write the file to be saved */
    			fp = fopen(right, "wb");
			if (fp != NULL) 
			{
				printf("File opened success\n");
			}
			nWritten = fwrite(buffer,1,nbytes,fp);
			totalReadC += nbytes;
			totalWrittenC += nWritten;
			while (1) // expect 1 successful conversion
			{
				bzero(buffer,sizeof(buffer));
				nbytes = recvMyPacketC(sock, buffer, MAXBUFSIZE, 0, (struct sockaddr *) &remote, &remote_length, (struct sockaddr *) &remote, remote_length);
				if (strcmp(buffer, STOP) == 0)
				{
					break;
				}
				nWritten = fwrite(buffer,1,nbytes,fp);
				totalReadC += nbytes;
				totalWrittenC += nWritten;
			}
			printf("File closed %d totalRead %d totalWritten %d\n",fclose(fp),totalReadC,totalWrittenC);
		}
		else if (strcmp(left, PUT) == 0)
		{
			/* open the file to be sent */
    			fp = fopen(right, "rb");
			if (fp == NULL) 
			{
				printf("File %s not found\n",right);
				continue;
			}
			nbytes = sendMyPacketC(sock, command, strlen(command), 0, (struct sockaddr *) &remote, remote_length,(struct sockaddr *) &remote, &remote_length);
			bzero(buffer,sizeof(buffer));
			while (fgets (buffer , MAXBUFSIZE , fp) != NULL) // expect 1 successful conversion
			{
				curr_pos = ftell(fp);
  				nbytes = sendMyPacketC(sock, buffer, curr_pos-last_pos, 0, (struct sockaddr *) &remote, remote_length,(struct sockaddr *) &remote, &remote_length);
				totalReadC += curr_pos-last_pos;
				totalWrittenC += nbytes;
				last_pos = curr_pos;
				bzero(buffer,sizeof(buffer));
			}
			if (feof(fp)) 
			{
  				// hit end of file
				nbytes = sendMyPacketC(sock, STOP, strlen(STOP), 0, (struct sockaddr *) &remote, remote_length,(struct sockaddr *) &remote, &remote_length);
			}
			printf("File closed %d totalRead %d totalWritten %d\n",fclose(fp),totalReadC,totalWrittenC);
		}
		else 
		{
			nbytes = sendMyPacketC(sock, command, strlen(command), 0, (struct sockaddr *) &remote, remote_length,(struct sockaddr *) &remote, &remote_length);
			bzero(buffer,sizeof(buffer));
			nbytes = recvMyPacketC(sock, buffer, MAXBUFSIZE, 0, (struct sockaddr *) &remote, &remote_length, (struct sockaddr *) &remote, remote_length);
			printf("%s\n", buffer);
		}
		printf("Request Complete totalPacketsSent %d totalPacketsReceived %d\n",totalPacketsSentC,totalPacketsReceivedC);
	}
	close(sock);
}

int sendMyPacketC(int sock, const void *buf, size_t len,int flags,const struct sockaddr *remote,int remote_length, struct sockaddr *restrict address,socklen_t *restrict address_len)
{
	char ack[MAXBUFSIZE];
	int bytesSent = 0;
	int bytesReceived = 0;
	do {
      		bytesSent = 0;
		bzero(ack,sizeof(ack));
		bytesSent = sendto(sock, buf, len, flags, remote, remote_length);
		totalPacketsSentC += 1;
		bytesReceived = recvfrom(sock, ack, MAXBUFSIZE, flags, address, address_len);
		totalPacketsReceivedC += 1;
	}while( strcmp(ack, ACKC) != 0 );
	return bytesSent;
}

int recvMyPacketC(int sock, void *restrict buffer, size_t length,int flags, struct sockaddr *restrict address,socklen_t *restrict address_len, const struct sockaddr *remote,int remote_length)
{
	bzero(buffer,sizeof(buffer));
	int bytesReceived = recvfrom(sock, buffer, MAXBUFSIZE, flags, address, address_len);
	totalPacketsReceivedC += 1;
	int bytesSent = sendto(sock, ACKC, strlen(ACKC), flags, remote, remote_length);
	totalPacketsSentC += 1;
	return bytesReceived;
}
