//By Jianming Sang
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

#define BUFSIZE 4096

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  transferclient [options]\n"                                                \
"options:\n"                                                                  \
"  -s                  Server (Default: localhost)\n"                         \
"  -p                  Port (Default: 8888)\n"                                \
"  -o                  Output file (Default foo.txt)\n"                       \
"  -h                  Show this help message\n"                              

/* Main ========================================================= */
int main(int argc, char **argv) {
    int option_char = 0;
    char *hostname = "localhost";
    unsigned short portno = 8888;
    char *filename = "foo.txt";

    // Parse and set command line arguments
    while ((option_char = getopt(argc, argv, "s:p:o:h")) != -1) {
        switch (option_char) {
            case 's': // server
                hostname = optarg;
                break; 
            case 'p': // listen-port
                portno = atoi(optarg);
                break;                                        
            case 'o': // filename
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
 
    //convert the unsinged short portno to string formation
    char port[7];
    sprintf(port, "%d", portno);

    /* Socket Code Here */

    //create socket using getaddrinfo function
    struct addrinfo addrHints;
    memset(&addrHints, 0, sizeof(addrHints));
    addrHints.ai_family = AF_INET;
    addrHints.ai_socktype = SOCK_STREAM;
    addrHints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *addrList;

    int r = getaddrinfo(hostname, port, &addrHints, &addrList);
    if(r != 0)
    {
        fprintf(stderr, "getaddrinfo errors");
        exit(1);
    }
    
    int clientSock = -1;
    struct addrinfo * addr;
    for (addr = addrList; addr != NULL; addr = addr->ai_next)
    {
        clientSock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if(clientSock < 0)
            continue;
        if(connect(clientSock, addr->ai_addr, addr->ai_addrlen) < 0)
        {
            close(clientSock);
            fprintf(stderr, "error in connection\n");
            continue;
        }
        break;
    }
    freeaddrinfo(addrList);
 

    if(clientSock < 0)
    {
        fprintf(stderr, "fail to creat client socket.\n");
        exit(2);
    }
    //create file for writing
    int file;
    if((file = open(filename, O_CREAT | O_RDWR, S_IRUSR|S_IWUSR)) < 0)
    {
        fprintf(stderr, "error in opening file.\n");
        exit(3);
    }
    ssize_t byteReceived;

    char buffer[BUFSIZE];
    //loop to make sure all message from the server will be received
    while((byteReceived = recv(clientSock, buffer, BUFSIZE, 0)) > 0)
    {
        if(write(file, buffer, byteReceived) < 0)
        {
            fprintf(stderr, "error in writing.\n");
            exit(1);
        }
    }

    close(file);
    close(clientSock);
    exit(0);
}
