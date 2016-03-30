//By Jianming Sang
//basic socket based echo client and server communication
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>

/* Be prepared accept a response of this length */
#define BUFSIZE 4096

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoclient [options]\n"                                                    \
"options:\n"                                                                  \
"  -s                  Server (Default: localhost)\n"                         \
"  -p                  Port (Default: 8888)\n"                                \
"  -m                  Message to send to server (Default: \"Hello World!\"\n"\
"  -h                  Show this help message\n"                              

/* Main ========================================================= */
int main(int argc, char **argv) {
    int option_char = 0;
    char *hostname = "localhost";
    unsigned short portno = 8888;
    char *message = "Hello World!";

    // Parse and set command line arguments
    while ((option_char = getopt(argc, argv, "s:p:m:h")) != -1) {
        switch (option_char) {
            case 's': // server
                hostname = optarg;
                break; 
            case 'p': // listen-port
                portno = atoi(optarg);
                break;                                        
            case 'm': // server
                message = optarg;
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

    struct addrinfo addrHints;
    memset(&addrHints, 0, sizeof(addrHints));
    addrHints.ai_family = AF_UNSPEC;
    addrHints.ai_socktype = SOCK_STREAM;
    addrHints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *addrList;
    //using getaddrifo to get the information for constructing socket
    int r = getaddrinfo(hostname, port, &addrHints, &addrList);
    if(r != 0)
    {
        fprintf(stderr, "getaddrinfo errors");
        exit(1);
    }
    
    int clientSock = -1;
    struct addrinfo * addr;
    //loop through the returned addr list until one valid socketed is returned.
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
 

    if(clientSock == -1)
    {
        fprintf(stderr, "create client socket failed");
        exit(1);
    }

    //receiving message
    size_t messageLen = strlen(message);
    ssize_t byteSent = send(clientSock, message, messageLen, 0);
    if(byteSent < 0)
    {
        fprintf(stderr, "send message failed");
        exit(1);
    }
    else if(byteSent != messageLen)
    {
        fprintf(stderr, "unexpected number of bytes message sent");
        exit(1);
    }
    //send message
    unsigned int byteReceived = 0;
    ssize_t numBytes;
    while(byteReceived < messageLen)
    {
        char buffer[BUFSIZE];
        numBytes = recv(clientSock, buffer, BUFSIZE-1, 0);
        if(numBytes < 0)
        {
            fprintf(stderr, "receive message failed");
            exit(1);
        }
        else if(numBytes == 0)
        {
            fprintf(stderr, "connections stops");
            exit(1);
        }
        
        byteReceived += numBytes;
        buffer[numBytes] = '\0';
        fprintf(stdout, "%s", buffer);
    }
    putc('\n', stdout);
    close(clientSock);
    exit(0);
}
