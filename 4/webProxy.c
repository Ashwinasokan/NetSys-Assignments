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
#include <time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <openssl/md5.h>
#include <math.h>

#define SIZE 1024
#define KEY_MAX_LENGTH (256)

int input_timeout (int filedes, unsigned int seconds) {
  fd_set set;
  struct timeval timeout;

  /* Initialize the file descriptor set. */
  FD_ZERO (&set);
  FD_SET (filedes, &set);

  /* Initialize the timeout data structure. */
  timeout.tv_sec = seconds;
  timeout.tv_usec = 0;

  /* select returns 0 if timeout, 1 if input available, -1 if error. */
  return select (FD_SETSIZE,&set, NULL, NULL,&timeout);
}

char *hash(char *string) {
	unsigned char *digest = (unsigned char *) malloc(sizeof(unsigned char) * 16);
	memset(digest, 0, sizeof(unsigned char) * 16);
	MD5_CTX context;
	MD5_Init(&context);
	MD5_Update(&context, string, strlen(string));
	MD5_Final(digest, &context);
	char *md5string = (char *) malloc(sizeof(char) * 33);
	memset(md5string, 0, sizeof(unsigned char) * 33);
	for(int i = 0; i < 16; ++i) {
		sprintf(&md5string[i*2], "%02x", (unsigned int)digest[i]);
	}
	md5string[32] =  '\0';
	return md5string;
}

int hostname_to_ip(char * hostname , char* ip) {
    struct hostent *he;
    struct in_addr **addr_list;
    int i;
    if ( (he = gethostbyname( hostname ) ) == NULL)  {
        // get the host info
        herror("gethostbyname");
        return 1;
    }
    addr_list = (struct in_addr **) he->h_addr_list;
    for(i = 0; addr_list[i] != NULL; i++)  {
        //Return the first one;
        strcpy(ip , inet_ntoa(*addr_list[i]) );
        return 0;
    }
    return 1;
}

long get_nano_time() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}
int is_present(char *path,double timeout) {
	struct stat sbuf;
	char *hash_path = hash(path);
	if (stat(hash_path, &sbuf) < 0) {
		return 0;
	}
	time_t now;
	time(&now);
	double diff = difftime(now,sbuf.st_mtime);
	if (diff < timeout) {
		return 1;
	}
	else {
		return 0;
	}
}
/* Read characters from 'fd' until a newline is encountered. If a newline
  character is not encountered in the first (n - 1) bytes, then the excess
  characters are discarded. The returned string placed in 'buf' is
  null-terminated and includes the newline character if it was read in the
  first (n - 1) bytes. The function return value is the number of bytes
  placed in buffer (which includes the newline character if encountered,
  but excludes the terminating null byte). */

ssize_t readLine(int fd, void *buffer, size_t n) {
	memset(buffer, 0, n);
    ssize_t numRead;                    /* # of bytes fetched by last read() */
    size_t totRead;                     /* Total bytes read so far */
    char *buf;
    char ch;
    if (n <= 0 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }
    buf = buffer;                       /* No pointer arithmetic on "void *" */
    totRead = 0;
    for (;;) {
        numRead = read(fd, &ch, 1);
        if (numRead == -1) {
            if (errno == EINTR)         /* Interrupted --> restart read() */
                continue;
            else
                return -1;              /* Some other error */
        } else if (numRead == 0) {      /* EOF */
            if (totRead == 0)           /* No bytes read; return 0 */
                return 0;
            else                        /* Some bytes read; add '\0' */
                break;
        } else {                        /* 'numRead' must be 1 if we get here */
            if (totRead < n - 1) {      /* Discard > (n - 1) bytes */
                totRead++;
                *buf++ = ch;
            }
            if (ch == '\n')
                break;
        }
    }
    *buf = '\0';
    return totRead;
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

int transfer(FILE *from,FILE *to) {
	int written = 0;
	char *buffer = (char *) malloc(sizeof(char) * SIZE);
	memset(buffer, 0, SIZE);
	int numRead = fread(buffer, 1,SIZE,from);
	while (numRead > 0) {
		int numSent = fwrite(buffer, 1,numRead,to);
		if(numRead != numSent) {
			printf("Error: Read %d Written %d\n",numRead,numSent);
			perror("Error:");
		}
		numRead = fread(buffer, 1,SIZE,from);
		written += numSent;
	}
	if(written == 0) {
		perror("Error: Written Zero bytes");
	}
	return written;
}

int serveGet(char *path,FILE *to) {
	char *hash_path = hash(path);
	FILE *from = fopen(hash_path, "r");
	if (from == NULL){
		printf("Error: on fdopen\n");
		return 0;
	}
    return transfer(from,to);
}

int saveGet(char * host,char *ip,int portNo,char * path) {
	char *hash_path = hash(path);
	FILE *to = fopen(hash_path,"w");
	int sockfd = get_socket(ip,portNo);
  	if(sockfd <= 0) {
		printf("Error: on sockfd\n");
		return 0;
	}
	FILE *from;
	if ((from = fdopen(sockfd, "r")) == NULL){
		printf("Error: on fdopen\n");
		return 0;
	}
	char command[SIZE];
	int n = sprintf (command,"GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, host);
    n = write(sockfd, command, n);
    n = transfer(from,to);
    fclose(to);
    return n;
}

char *new() {
	char *buffer = (char *) malloc(sizeof(char) * SIZE);
	memset(buffer, 0, SIZE);
	return buffer;
}

void extract(FILE *from,int timeout) {
	char *buffer = new();
	int numRead = fread(buffer, 1,SIZE,from);
	while (numRead > 0) {
		char *start = strstr (buffer,"http://");
		while (start != NULL) {
			char *end = strchr(start,'"');
			if (end != NULL) {
				int length = end - start;
				char *h = new();
				char *p = new();
				char *i = new();
				sprintf(p,"%.*s", length, start);
				sscanf(p, "http://%[^/]", h);
				if(hostname_to_ip(h,i) == 0) {
					saveGet(h,i,80,p);
					//printf("Prefetched => path:%s  Written:%d\n",p,n);
				}
				start = strstr (end,"http://");
			}
			else {
				start = end;
			}
		}
		numRead = fread(buffer, 1,SIZE,from);
	}
}

int prefetch(char * host,char *ip,int portNo,char * path,int timeout) {
	int sockfd = get_socket(ip,portNo);
  	if(sockfd <= 0) {
		printf("Error: on sockfd\n");
		return 0;
	}
	FILE *from;
	if ((from = fdopen(sockfd, "r")) == NULL){
		printf("Error: on fdopen\n");
		return 0;
	}
	char command[SIZE];
	int n = sprintf (command,"GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, host);
    n = write(sockfd, command, n);
    extract(from,timeout);
    fclose(from);
    return n;
}

int main (int argc, char * argv[] ) {
	if (argc != 3) {
		printf ("USAGE:  portNo timeOut\n");
		exit(1);
	}
	int portno = atoi(argv[1]);
	double timeout = atof(argv[2]);
	int parentfd,childfd;
	pid_t childpid;
	pid_t fetchpid;
	struct sockaddr_in serveraddr; 
  	struct sockaddr_in clientaddr; 
	int clientlen;          
	char buf[SIZE];     
  	char method[SIZE];
	char host[SIZE];
	char path[SIZE];
	char ip[SIZE];
	char protocol[SIZE];
	const char *localhost = "localhost";
	if ((parentfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("unable to open socket\n");
		exit(1);
	}
	int optval = 1;
  	setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

  	bzero((char *) &serveraddr, sizeof(serveraddr));
  	serveraddr.sin_family = AF_INET;
  	struct hostent *server = gethostbyname(localhost);
  	if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(1);
    }
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr,server->h_length);
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
  			FILE *stream = fdopen(childfd, "w");
    		if (stream == NULL){
				printf("ERROR on fdopen\n");
				exit(1);
			}
			int keepAlive = 0;
			do {
				readLine(childfd, buf, SIZE);
				sscanf(buf, "%s %s %s ", method, path,protocol);
				keepAlive = protocol[strlen(protocol)-1] - '0';
				memset(host, 0, SIZE);
				sscanf(path, "http://%[^/]", host);
				if (strcasecmp(method, "GET") == 0) {
					long before = get_nano_time();
					if ( (fetchpid = fork ()) == 0 ) { //if it’s 0, it’s prefetch process
						fclose(stream);
						if(hostname_to_ip(host , ip) == 0) {
							prefetch(host,ip,80,path,timeout);
						}
						exit(0);
					}
					if (is_present(path,timeout) == 0) {
						if(hostname_to_ip(host , ip) == 0) {
							saveGet(host,ip,80,path);
							// printf("Cached => path:%s  Written:%d\n",path,n);
						}
					}
					int n = serveGet(path,stream);
					long after = get_nano_time();
					printf("Served => path:%s  Written:%d time:%ld micro seconds\n",path,n,(after-before)/1000);
				}
				else {
					printf("Error => method: %s host:%s path:%s\n",method,host,path);		
				}
				fflush(stream);
			}while(input_timeout(childfd,timeout) == 1 && keepAlive == 1);
			fclose(stream);
			exit(0);
		}
    	close(childfd);
	} 
}
