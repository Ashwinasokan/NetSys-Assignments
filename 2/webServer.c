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
#include <sys/mman.h>
#include <sys/stat.h>
#define SIZE 1024

void sendErrorMessage(FILE *stream, char *cause, char *errno, char *shortmsg, char *longmsg) {
  fprintf(stream, "HTTP/1.1 %s %s\n", errno, shortmsg);
  fprintf(stream, "Content-type: text/html\n");
  fprintf(stream, "\n");
  fprintf(stream, "<html><title>Error</title>");
  fprintf(stream, "<body bgcolor=""ffffff"">\n");
  fprintf(stream, "%s: %s\n", errno, shortmsg);
  fprintf(stream, "<p>%s: %s\n", longmsg, cause);
  fprintf(stream, "<hr><em>Web server</em>\n");
  fflush(stream);
}

int main (int argc, char * argv[] ) {
	int portno;
	FILE *fp;
	pid_t childpid;
	char left[100], right[100],cwd[100],directoryIndex[100];
	int parentfd;          
	int childfd;           
	int optval = 1;        
	struct sockaddr_in serveraddr; 
  	struct sockaddr_in clientaddr; 
	int clientlen;         
	struct hostent *hostp; 
  	char *hostaddrp;       
	FILE *stream;          
	char buf[SIZE];     
  	char method[SIZE];  
  	char uri[SIZE];     
  	char version[SIZE]; 
  	char filename[SIZE];
  	char filetype[SIZE];
	struct stat sbuf;      
	int fd;
	char *p;               
	char *message;
	int contentLength;
	fp = fopen("ws.conf", "rb");
    	if (fp == NULL) {
		printf("Not able to read ws.conf\n");
    		exit(1);
	}
	while (fscanf(fp, "%s %s", left, right) > 0) {
		if(left[0] == '#') {
			continue;
		}
		else if (strcmp(left, "Listen") == 0) {
			portno = atoi(right);
			printf("Read port no %d\n",portno);
			if(portno < 1024) {
				printf("Port no %d below 1024 is not valid\n",portno);
				exit(1);
			}
		}
		else if (strcmp(left, "DocumentRoot") == 0) {
			getcwd(cwd, sizeof(cwd));
			if(chdir(right) == 0) {
				printf("Changed working directory from %s to %s\n",cwd,right);	
			}
			else {
				getcwd(cwd, sizeof(cwd));
				printf("Not able to change working directory from %s to %s\n",cwd,right);
				exit(1);
			}
		}
		else if (strcmp(left, "DirectoryIndex") == 0) {
			strcpy(directoryIndex, right);
			printf("Read directory Index %s\n",directoryIndex);
		}
	}
	if ((parentfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("unable to open socket\n");
		exit(1);
	}
	fprintf(stderr, "Opened socket %d\n",parentfd);

  	setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

  	bzero((char *) &serveraddr, sizeof(serveraddr));
  	serveraddr.sin_family = AF_INET;
  	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  	serveraddr.sin_port = htons((unsigned short)portno);
  	if (bind(parentfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
		printf("ERROR on binding\n");
		exit(1);
	}

  	if (listen(parentfd, 10) < 0) {
		printf("ERROR on listen\n");
		exit(1);
	} 

  	while (1) {
		clientlen = sizeof(clientaddr);
    		childfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
    		if (childfd < 0) {
			printf("ERROR on accept\n");
			continue;
		}
		if ( (childpid = fork ()) == 0 ) { //if it’s 0, it’s child process

  			close (parentfd);

    			if ((stream = fdopen(childfd, "r+")) == NULL){
				printf("ERROR on fdopen\n");
				exit(1);
			}

    			fgets(buf, SIZE, stream);
    			printf("%s", buf);
    			sscanf(buf, "%s %s %s\n", method, uri, version);

    			if (strcasecmp(method, "GET") == 0) {
    				fgets(buf, SIZE, stream);
    				printf("%s", buf);
    				while(strcmp(buf, "\r\n")) {
      					fgets(buf, SIZE, stream);
      					printf("%s", buf);
    				}

				strcpy(filename, ".");
      				strcat(filename, uri);
      				if (uri[strlen(uri)-1] == '/') 
					strcat(filename, directoryIndex);
		
    				if (stat(filename, &sbuf) < 0) {
					printf("Couldn't find the file %s\n",filename);
					message = (char*)malloc(100 * sizeof(char));
					sprintf(message, "URL does not exist: %s", filename);
					sendErrorMessage(stream, filename, "404", "Not found", message);
      					fclose(stream);
      					exit(0);
    				}

				if (strstr(filename, ".htm"))
					strcpy(filetype, "text/html");
				else if (strstr(filename, ".png"))
					strcpy(filetype, "image/png");
      				else if (strstr(filename, ".gif"))
					strcpy(filetype, "image/gif");
      				else if (strstr(filename, ".jpg"))
					strcpy(filetype, "image/jpg");
				else if (strstr(filename, ".ico"))
					strcpy(filetype, "image/x-icon");
				else if (strstr(filename, ".css"))
					strcpy(filetype, "text/css");
				else if (strstr(filename, ".js"))
					strcpy(filetype, "text/javascript");
      				else 
					strcpy(filetype, "text/plain");

      				fprintf(stream, "HTTP/1.1 200 OK\n");
      				fprintf(stream, "Server: Simple HTTP Web Server\n");
      				fprintf(stream, "Content-length: %d\n", (int)sbuf.st_size);
      				fprintf(stream, "Content-type: %s\n", filetype);
      				fprintf(stream, "\r\n"); 
      				fflush(stream);

      				fd = open(filename, O_RDONLY);
      				p = mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
      				fwrite(p, 1, sbuf.st_size, stream);
      				munmap(p, sbuf.st_size);
    				fclose(stream);
				exit(0);
			}
			else if (strcasecmp(method, "PUT") == 0) {
				fgets(buf, SIZE, stream);
    				printf("%s", buf);
    				while(strcmp(buf, "\r\n")) {
      					fgets(buf, SIZE, stream);
      					printf("%s", buf);
					if (strncasecmp(buf, "Content-Length",12) == 0) {
						sscanf(buf, "%s %s\n", left, right);
						contentLength = atoi(right);
						printf("Found contentLength %d\n", contentLength);
					}
    				}

				strcpy(filename, ".");
      				strcat(filename, uri);
      				if (uri[strlen(uri)-1] == '/') 
					strcat(filename, directoryIndex);
				
				fp = fopen(filename, "w+");
				int read = 1024,total = 0;
				while (total < contentLength) {
					if(contentLength-total < read) {
						read = contentLength-total;
					}
					fgets(buf, read, stream);
					fwrite(buf,1,read,fp);
					total += read;	
				}
				fprintf(stream, "HTTP/1.1 200 OK\n");
      				fprintf(stream, "Server: Simple HTTP Web Server\n");
      				fprintf(stream, "Content-type: %s\n", "text/plain");
      				fprintf(stream, "\r\n"); 
				fclose(stream);
				exit(0);
			}
			else {
				printf("Not Implemented we do not support this method %s\n",method);
				message = (char*)malloc(100 * sizeof(char));
				sprintf(message, "%s method not implemented", method);
      				sendErrorMessage(stream, method, "501", "Not Implemented", message);
      				fclose(stream);
      				exit(0);
    			}
		}
    		close(childfd);
	}
}
