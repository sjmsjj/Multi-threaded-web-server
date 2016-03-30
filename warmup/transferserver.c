#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#if 0
/* 
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;   /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

#define BUFSIZE 4096

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  transferserver [options]\n"                                                \
"options:\n"                                                                  \
"  -p                  Port (Default: 8888)\n"                                \
"  -f                  Filename (Default: bar.txt)\n"                         \
"  -h                  Show this help message\n"                              

int main(int argc, char **argv) {
  int option_char;
  int portno = 8888; /* port to listen on */
  char *filename = "bar.txt"; /* file to transfer */
  int maxnpending = 5;

  // Parse and set command line arguments
  while ((option_char = getopt(argc, argv, "p:f:h")) != -1){
    switch (option_char) {
      case 'p': // listen-port
        portno = atoi(optarg);
        break;                                        
      case 'f': // listen-port
        filename = optarg;
        break;   
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;       
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
    }
  }

  char port[7];
  sprintf(port, "%d", portno);
  /* Socket Code Here */

  //create server socket
  int servSock = socket(AF_INET, SOCK_STREAM, 0);
  
  int optval = 1;
  int opt = setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
  struct sockaddr_in servAddr;
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(portno);
  
  if(bind(servSock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0)
  {
    fprintf(stderr, "error in bind\n");
    exit(1);
  }
  if(listen(servSock, maxnpending) < 0)
  {
    fprintf(stderr, "error in listen\n");
    exit(1);
  }

 //loop forever
  while(1)
  {
    unsigned int totalBytes = 0;
    struct sockaddr_storage sockTmp;
    socklen_t sockLen = sizeof(sockTmp);
    int sock = accept(servSock, &sockTmp, &sockLen);
    if(sock < 0)
      continue;

    char buffer[BUFSIZE];
    //open file for reading message
    int file;
    if((file = open(filename, S_IRUSR)) < 0)
    {
      fprintf(stderr, "error in opening file.\n");
      exit(4);
    }
  
    ssize_t byteSent = 0 , byteRead = 0, temp;
    //loop to make sure all information from read will be sent
    while((byteRead = read(file, buffer, BUFSIZE)) > 0)
    {
      byteSent = 0;
      while(byteSent < byteRead)
      {
        temp = send(sock, buffer + byteSent, byteRead - byteSent, 0);
        byteSent += temp;
      }
    }
    close(sock);
    close(file);
  }
  exit(0);
}
