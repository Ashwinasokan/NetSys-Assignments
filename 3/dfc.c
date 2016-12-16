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
#include <math.h>
#define SIZE 1024
#define KEY_MAX_LENGTH (256)
typedef struct LISTING
{
    char filename[SIZE];
    int present[4];
    char *contents[4];
    int contents_size[4];
} LS;
int count_pieces(struct LISTING listing) {
	return listing.present[0] + listing.present[1] + listing.present[2] + listing.present[3];
}
int hash(char *filename) {
    int bytes;
    unsigned char data[1024];
    int md5hash;
    unsigned char c[MD5_DIGEST_LENGTH];
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext; 
    if (inFile == NULL) {
        printf ("%s not found\n", filename);
        return 0;
    }
    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0){
        MD5_Update (&mdContext, data, bytes);
    }
    MD5_Final (c,&mdContext);
    md5hash = c[MD5_DIGEST_LENGTH - 1] % 4;
    fclose (inFile);
    return md5hash;
}
char* xorencrypt(char *string, int length,char *key){
    int key_length = strlen(key);
    int i = 0;
    while(i < length)
    {
        string[i] = string[i] ^ key[i % key_length];
        i++;
    }
    return string;
}
int filesize(char *filename) {
    struct stat sbuf;
    if (stat(filename, &sbuf) < 0) {
		printf ("Error: %s not found\n", filename);
        return 0;
	}
	return sbuf.st_size;
}
char * get_piece_name(char *filename,int piece) {
	char *piece_name = (char *) malloc(sizeof(char) * SIZE);
	sprintf (piece_name, ".%s.%d", filename,piece);
	return piece_name;
}
int get_socket(char * ip_address,int portNo) {
	/* Construct remote_addr struct */
  	struct sockaddr_in remote_addr;
  	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	inet_pton(AF_INET, ip_address, &(remote_addr.sin_addr));
	remote_addr.sin_port = htons(portNo);
	/* Construct local_addr struct */
	struct sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	local_addr.sin_port = htons(0);
	/* open socket descriptor */
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error: unable to open socket %s portNo %d\n",ip_address,portNo);
		return 0;
	}
	int optval = 1;
	/* allows us to restart server immediately */
  	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
  	/* Bind the socket to local address*/
	int rc = bind(sockfd, (struct sockaddr *) &local_addr, sizeof(local_addr));
	/* Check whether socket is bound correctly */
	if(rc < 0) {
		printf("Error: cannot bind on TCP port %s :%d \n",ip_address,portNo);
		close(sockfd);
		return 0;
	}
	/* Connect the socket to remote address*/
	rc = connect(sockfd, (struct sockaddr *) &remote_addr, sizeof(remote_addr));
	/* Check whether socket is connect correctly */
    if(rc < 0) {
        printf("Error: cannot connect on TCP port %s :%d \n",ip_address,portNo);
        close(sockfd);
        return 0;
    }
    return sockfd;
}
int minimum(int a,int b) {
	if (a < b){
		return a;
	}
	else {
		return b;
	}
}
int transfer(FILE *from,FILE *to,int length,char *filename) {
	int written = 0;
	int to_write = length;
	char *buffer = (char *) malloc(sizeof(char) * SIZE);
	memset(buffer, 0, SIZE);
	while (to_write > 0) {
		int toRead = minimum(SIZE, to_write);
		int numRead = fread(buffer, 1,toRead,from);
		buffer = xorencrypt(buffer, numRead,filename);
		int numSent = fwrite(buffer, 1,numRead,to);
		to_write -= numSent;
		written += numSent;
	}
	if(length != written) {
		printf("Error: Requested %d Transferred %d\n",length,written);
	}
	return written;
}
int put(char * ip_address,int portNo,char *user_name,char *password,char *directory,char *filename,int piece,int start,int size) {
  	int sockfd = get_socket(ip_address,portNo);
  	if(sockfd <= 0) {
		return 0;
	}
	FILE *stream;
	if ((stream = fdopen(sockfd, "r+")) == NULL){
		printf("Error: on fdopen\n");
		return 0;
	}
    char command[SIZE];
    int n = sprintf (command, "%s %s PUT %s %s\n",user_name,password,directory,get_piece_name(filename,piece));
    int rc = write(sockfd, command, n); 
    n = sprintf (command, "%d\n", size);
    rc = write(sockfd, command, n); 
    FILE *fd = fopen(filename, "r");
    fseek ( fd , start , SEEK_SET );
    rc = transfer(fd,stream,size,filename);
    if (rc != size) {
      printf("Error: incomplete transfer %d of %d bytes\n",rc,size);
      return rc;
    }
    /* close descriptor for file that was sent */
    fclose(fd);
	fclose(stream);
    /* close socket descriptor */
    close(sockfd);
    return rc;
}
int writeFile(struct LISTING listing,char *filename){
	int count = count_pieces(listing);
	if (count != 4) {
		return 0;
	}
	int fw = open(filename, O_CREAT|O_WRONLY,S_IRWXU|S_IRWXG|S_IRWXO);
	int numWrite = 0;
    int totalWritten = 0;
	for(int i=0;i<4;i++){
		numWrite = write(fw,listing.contents[i],listing.contents_size[i]);
		totalWritten += numWrite;
	}
	return 1;
}
int copy(FILE *from,char *to,int length,char *filename) {
	int written = 0;
	int to_write = length;
	char *buffer = (char *) malloc(sizeof(char) * SIZE);
	memset(buffer, 0, SIZE);
	while (to_write > 0) {
		int toRead = minimum(SIZE, to_write);
		int numRead = fread(buffer, 1,toRead,from);
		buffer = xorencrypt(buffer, numRead,filename);
		memcpy(to+written, buffer, numRead);
		to_write -= numRead;
		written += numRead;
	}
	if(length != written) {
		printf("Error: Requested %d Transferred %d\n",length,written);
	}
	return written;
}
struct LISTING get(char * ip_address,int portNo,char *user_name,char *password,char *directory,char *filename,struct LISTING listing) {
	int sockfd = get_socket(ip_address,portNo);
  	if(sockfd <= 0) {
		return listing;
	}
	FILE *stream;
	if ((stream = fdopen(sockfd, "r+")) == NULL){
		printf("ERROR on fdopen\n");
		return listing;
	}
	char command[SIZE];
    int n = sprintf (command, "%s %s GET %s %s\n", user_name,password,directory,filename);
    write(sockfd, command, n);
    int numRead = 0;
    int totalRead = 0;
    int piece = 0;
	int size = 0;
    while((numRead = fscanf(stream, "%d %d\n", &piece,&size))> 0) {
		if (piece == -1) {
			break;
		}
		listing.contents_size[piece] = size;
		listing.contents[piece] = (char *) malloc(sizeof(char) * size);
		numRead = copy (stream,listing.contents[piece],size,filename);
		listing.present[piece] = 1;
		totalRead += numRead;
	}
	printf("Received file: %s from ip: %s port: %d bytes: %d\n",filename,ip_address,portNo,totalRead);
	fclose(stream);
	close(sockfd);
	return listing;
}
int add_listing(char *filename,int length,struct LISTING *listings,int size){
	int piece = filename[strlen(filename)-1] - '0';
	char name[SIZE];
	strncpy ( name, filename+1, strlen(filename)-3);
	name[strlen(filename)-3] = '\0';
	int placed = 0;
	for(int i=0;i<size;i++){
		if (strcasecmp(name, listings[i].filename) == 0) {
			listings[i].present[piece] = 1;
			placed = 1;
		}
	}
	if (placed != 1) {
		strcpy(listings[size].filename, name);
		listings[size].present[piece] = 1;
		size += 1;
	}
	return size;
}
int list(char * ip_address,int portNo,char *user_name,char *password,char *directory,struct LISTING *listings,int size) {
	int sockfd = get_socket(ip_address,portNo);
  	if(sockfd <= 0) {
		return size;
	}
	FILE *stream;
	if ((stream = fdopen(sockfd, "r+")) == NULL){
		printf("ERROR on fdopen\n");
		return size;
	}
	char command[SIZE];
	char *buf = (char *) malloc(sizeof(char) * SIZE);
	memset(buf, 0, SIZE);
    int n = sprintf (command, "%s %s LIST %s\n", user_name,password,directory);
    write(sockfd, command, n);
    int numRead = 0;
    int totalRead = 0;
    while((numRead = fscanf (stream, "%s\n", buf))> 0) {
		totalRead += numRead;
		size = add_listing(buf,numRead,listings,size);
	}
	close(sockfd);
	return size;
}
char* make_directory(char * ip_address,int portNo,char *user_name,char *password,char *directory,char *filename) {
	char *buf = (char *) malloc(sizeof(char) * SIZE);
	memset(buf, 0, SIZE);
	int sockfd = get_socket(ip_address,portNo);
  	if(sockfd <= 0) {
		sprintf (buf, "Error getting socket\n");
		return buf;
	}
	FILE *stream;
	if ((stream = fdopen(sockfd, "r+")) == NULL){
		printf("Error on fdopen\n");
		sprintf (buf, "Error getting socket\n");
		return buf;
	}
	char command[SIZE];
    int n = sprintf (command, "%s %s MKDIR %s %s\n", user_name,password,directory,filename);
    write(sockfd, command, n);
    fscanf (stream, "%s\n", buf);
    close(sockfd);
    return buf;
}
int decrementmd5(int a) {
	if (a == 0) {
		return 3;
	}
	else {
		return a-1;
	}
}
int main (int argc, char * argv[] ) {
	int portNo[4];
	char ip_address[4][KEY_MAX_LENGTH];
	char left[KEY_MAX_LENGTH], right[KEY_MAX_LENGTH], command[KEY_MAX_LENGTH], directory[KEY_MAX_LENGTH],file_name[KEY_MAX_LENGTH],user_name[KEY_MAX_LENGTH], password[KEY_MAX_LENGTH];
	if (argc != 2)
	{
		printf ("USAGE:  <conf>\n");
		exit(1);
	}
	FILE *fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		printf("Error: Not able to read %s\n",argv[1]);
		exit(1);
	}
	for(int i=0;i<4;i++){
		fscanf(fp, "%s %s", left, right);
		strcpy(ip_address[i], left);
		portNo[i] = atoi(right);
	}
	char input[SIZE];
	fscanf(fp, "%s %s", user_name, password);
	printf("Client [user: %s password: %s]\n",user_name,password);
	while (scanf ("%[^\n]%*c", input) > 0) {
		sscanf(input,"%s %s %s", command, directory,file_name);
		if (strcasecmp(command, "GET") == 0) {
			printf("Received command: %s Directory: %s Filename: %s\n",command, directory,file_name);
			struct LISTING listing = {0};
			listing = get(ip_address[0],portNo[0],user_name,password,directory,file_name,listing);
			if (count_pieces(listing) == 2) {
				listing = get(ip_address[2],portNo[2],user_name,password,directory,file_name,listing);
				if (count_pieces(listing) == 2) {
					listing = get(ip_address[1],portNo[1],user_name,password,directory,file_name,listing);
					listing = get(ip_address[3],portNo[3],user_name,password,directory,file_name,listing);
				}
			}
			else {
				listing = get(ip_address[1],portNo[1],user_name,password,directory,file_name,listing);
				if (count_pieces(listing) == 2) {
					listing = get(ip_address[3],portNo[3],user_name,password,directory,file_name,listing);
				}
			}
			printf("Finished command: file: %s no pieces: %d writtenFile: %d \n",file_name,count_pieces(listing),writeFile(listing,file_name));
		}
		else if (strcasecmp(command, "PUT") == 0) {
			printf("Received command: %s Directory: %s Filename: %s\n",command, directory,file_name);
			int md5 = hash(file_name);
			int size = filesize(file_name);
			int piece_size = (int)ceil((double)size/(double)4);
			for(int i=md5,count=0,seek=0;count<4;i = (i+1)%4,count++){
				int sent = put(ip_address[i],portNo[i],user_name,password,directory,file_name,count,seek,minimum(piece_size,size-seek));
				printf("PUT[ip: %s port: %d Directory: %s Filename: %s bytes: %d]\n",ip_address[i],portNo[i],directory,file_name,sent);
				seek += sent;
			}
			for(int i=decrementmd5(md5),count=0,seek=0;count<4;i = (i+1)%4,count++){
				int sent = put(ip_address[i],portNo[i],user_name,password,directory,file_name,count,seek,minimum(piece_size,size-seek));
				printf("PUT[ip: %s port: %d Directory: %s Filename: %s bytes: %d]\n",ip_address[i],portNo[i],directory,file_name,sent);
				seek += sent;
			}
			printf("Finished command\n");
		}
		else if (strcasecmp(command, "LIST") == 0) {
			printf("Received command: %s Directory: %s \n",command, directory);
			struct LISTING results[KEY_MAX_LENGTH]= {0};
			int size = 0;
			for(int i=0;i<4;i++){
				size = list(ip_address[i],portNo[i],user_name,password,directory,results,size);
			}
			if (size == 0) {
				printf("Empty\n");
			}
			for(int i=0;i<size;i++){
				printf("%s",results[i].filename);
				int complete = count_pieces(results[i]);
				if (complete != 4) {
					printf(" [incomplete]");
				}
				printf("\n");
			}
			printf("Finished command\n");
		}
		else if (strcasecmp(command, "MKDIR") == 0) {
			printf("Received command: %s Directory: %s Filename: %s\n",command, directory,file_name);
			for(int i=0;i<4;i++){
				printf("MKDIR[ip: %s result: %s]\n",ip_address[i],make_directory(ip_address[i],portNo[i],user_name,password,directory,file_name));
			}
			printf("Finished command\n");
		}
		else {
			printf("Not Implemented %s\n",command);		
		}
	}
}
