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
#include <assert.h>
#include "hashmap.h"
#include <openssl/md5.h>
#define SIZE 1024
typedef struct data_struct_s
{
    char key_string[SIZE];
    char value_string[SIZE];
} data_struct_t;
int minimum(int a,int b) {
	if (a < b){
		return a;
	}
	else {
		return b;
	}
}
int transfer(FILE *from,FILE *to,int length) {
	int written = 0;
	int to_write = length;
	char *buffer = (char *) malloc(sizeof(char) * SIZE);
	memset(buffer, 0, SIZE);
	while (to_write > 0) {
		int toRead = minimum(SIZE, to_write);
		int numRead = fread(buffer, 1,toRead,from);
		int numSent = fwrite(buffer, 1,numRead,to);
		to_write -= numSent;
		written += numSent;
	}
	if(length != written) {
		printf("Error: Requested %d Transferred %d\n",length,written);
	}
	return written;
}
int isDirectory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}
char *concatenate(char *a,char *b) {
	char *ab = (char *) malloc(sizeof(char) * SIZE);
	memset(ab, 0, SIZE);
	strcpy(ab,a);
	strcat(ab,b);
	return ab;
}
int list(int portno,char *dirname,FILE *to) {
	DIR *dp = opendir (dirname);
	if (dp == NULL) {
		printf("Error: Not able to read dirname %s\n",dirname);
		return 0;
	}
	int written = 0;
	struct dirent *ep;
	while ((ep = readdir (dp))) {
		if (strcasecmp(".", ep->d_name) == 0) { 
			continue;
		}
		if (strcasecmp("..", ep->d_name) == 0) { 
			continue;
		}
		char *path = concatenate(dirname,ep->d_name);
		if(isDirectory(path) == 1) {
			int numSent = fprintf (to, ".%s.%d\n", ep->d_name,portno-10001);
			written += numSent;
		}
        else {
			int numSent = fprintf (to, "%s\n", ep->d_name);
			written += numSent;
		}
	}
    (void) closedir (dp);
	return written;
}
char * make_directory(char *directory){
	char *cwd = (char *) malloc(sizeof(char) * SIZE);
	memset(cwd, 0, SIZE);
	cwd = getcwd(cwd, SIZE);
	strcat(cwd, directory);
	mkdir(cwd, ACCESSPERMS);
	return cwd;
}
int main (int argc, char * argv[] ) {
	if (argc != 3)
	{
		printf ("USAGE:  <location> <port>\n");
		exit(1);
	}
	int error;
	int parentfd;          
	int childfd;
	pid_t childpid;
	int portno = atoi(argv[2]);
	char left[SIZE], right[SIZE],directory[SIZE];
	map_t users = hashmap_new();
	data_struct_t* value;          
	int optval = 1;        
	struct sockaddr_in serveraddr; 
  	struct sockaddr_in clientaddr; 
	int clientlen;
	FILE *stream;          
	char buf[SIZE];     
  	char method[SIZE];  
  	char uri[SIZE];     
  	char user[SIZE]; 
	char password[SIZE];
  	char filename[SIZE];
	struct stat sbuf;
	FILE *fp = fopen("dfs.conf", "rb");
	snprintf(directory, SIZE, "%s", argv[1]);
    	if (fp == NULL) {
		printf("Error: Not able to read dfs.conf\n");
    		exit(1);
	}
	while (fscanf(fp, "%s %s", left, right) > 0) {
		value = malloc(sizeof(data_struct_t));
		snprintf(value->key_string, SIZE, "%s", left);
		snprintf(value->value_string, SIZE, "%s", right);
        	error = hashmap_put(users, value->key_string, value);
        	assert(error==MAP_OK);
	}
	char *cwd = make_directory(directory);
	printf("Server[PortNo: %d Directory: %s CWD: %s]\n",portno,directory,cwd);
	if ((parentfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error: unable to open socket\n");
		exit(1);
	}
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
    	childfd = accept(parentfd, (struct sockaddr *) &clientaddr,(socklen_t *) &clientlen);
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
			printf("Received command: %s\n",buf);
    		sscanf(buf, "%s %s %s %s %s\n", user,password,method,directory,uri);
			error = hashmap_get(users, user, (void**)(&value));
			assert(error==MAP_OK);
			if (strcasecmp(value->value_string, password) == 0){
				strcpy(filename, cwd);
				strcat(filename, "/");
				strcat(filename, user);
				mkdir(filename, ACCESSPERMS);
				if (strcasecmp(method, "GET") == 0) {
					strcat(filename, directory);
					char piece_name[SIZE];
					for(int i=0;i<4;i++){
						sprintf(piece_name, "%s.%s.%d", filename,uri,i);
						/* make sure the file exists */
						if (stat(piece_name, &sbuf) < 0) {
							printf("Warning: Couldn't find the file %s\n",piece_name);
						}
						else {
							int n = sprintf(buf, "%d %ld\n",i,sbuf.st_size);
							fwrite(buf, 1,n,stream);
							FILE *fw = fopen(piece_name, "r");
							int written = transfer(fw,stream,sbuf.st_size);
							fclose(fw);
							printf("Returned Filename: %s bytes: %d\n",piece_name,written);
						}
					}
					int n = sprintf(buf, "-1 -1\n");
					fwrite(buf, 1,n,stream);
				}
				else if (strcasecmp(method, "PUT") == 0) {
					strcat(filename, directory);
					strcat(filename, uri);
					fgets(buf, SIZE, stream);
					int size = atoi(buf);
					FILE *fw = fopen(filename, "w+");
					int written = transfer(stream,fw,size);
					fclose(fw);
					printf("Received Filename: %s bytes: %d\n",filename,written);
				}
				else if (strcasecmp(method, "LIST") == 0) {
					strcat(filename, directory);
					int written = list(portno,filename,stream);
					printf("Listed Filename: %s bytes: %d\n",filename,written);
				}
				else if (strcasecmp(method, "MKDIR") == 0) {
					strcat(filename, directory);
					strcat(filename, uri);
					int result = mkdir(filename, ACCESSPERMS);
					int n = sprintf(buf, "success\n");
					fwrite(buf, 1,n,stream);
					printf("Mkdir %s %d\n",filename,result);
				}
				else {
					printf("Error: Not Implemented we do not support this method %s\n",method);		
				}
			}
			else {
				printf("Warning: invalid password %s %s\n",password,value->value_string);
			}
			printf("Finished command\n");
			fclose(stream);
			exit(0);
		}
    		close(childfd);
	} 
}
