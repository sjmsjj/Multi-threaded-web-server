//By Jianming Sang

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
#include "gfserver.h"
/* 
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */


typedef struct gfserver_t
{
    unsigned int port;
    int maxnpending;
    ssize_t (*handler)(gfcontext_t*, char*, void*);
    void* arg;
} gfserver_t;

typedef struct gfcontext_t
{
    size_t sock;
} gfcontext_t;

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len){
    char str_status[200];
    char header[1000];
    int abort = 1; //signal whether to call gfs_abort
    //convert the status to string format
    if(status == GF_OK)
    {
        strcpy(str_status, "OK");
        abort = 0;  //if status is OK, keep working 
    }   
    else if(status == GF_FILE_NOT_FOUND)
    {
        strcpy(str_status, "FILE_NOT_FOUND");
        file_len = 0;
    }
    else if(status == GF_ERROR)
    {
        strcpy(str_status, "ERROR");
        file_len = 0;
    }
    else
    {
        fprintf(stderr, "error status.\n");
        gfs_abort(ctx);
        return -1;
    }

    //construct header information
    if(file_len == 0)
        sprintf(header,"%s %s%s", "GETFILE", str_status, "\r\n\r\n");
    else
        sprintf(header,"%s %s %d%s", "GETFILE", str_status, file_len, "\r\n\r\n");

    ssize_t headerLen = strlen(header);
    ssize_t byteSent = 0, byteSentTotal = 0;
    //send header informtion
    while(byteSentTotal < headerLen)
    {
       byteSent = write(ctx->sock, header + byteSentTotal, headerLen - byteSentTotal);
        if(byteSent <= 0)
            break;
       byteSentTotal += byteSent;
    }
    //if status is not ok, abort the job
    if(abort==1)
        gfs_abort(ctx);
    return byteSentTotal;
}

ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len){
    ssize_t byteSentTotal = 0, byteSent = 0;
    byteSentTotal = write(ctx->sock, data, len);
    while(byteSentTotal < len)
    {
        byteSent = write(ctx->sock, data + byteSentTotal, len - byteSentTotal);
        if(byteSent <= 0)
            break; //sending finishes or error happens here.
        byteSentTotal += byteSent;
    }
    return byteSentTotal;

}

void gfs_abort(gfcontext_t *ctx){
    close(ctx->sock);
}

gfserver_t* gfserver_create(){
    gfserver_t* gfs = malloc(sizeof(gfserver_t));
    if(gfs == NULL)
    {
        fprintf(stderr, "error in creating gfserver.\n");
        exit(1);
    }
    return gfs;
}

void gfserver_set_port(gfserver_t *gfs, unsigned short port){
    gfs->port = port;
}
void gfserver_set_maxpending(gfserver_t *gfs, int max_npending){
    gfs->maxnpending = max_npending;
}

void gfserver_set_handler(gfserver_t *gfs, ssize_t (*handler)(gfcontext_t *, char *, void*)){
    gfs->handler = handler;
}

void gfserver_set_handlerarg(gfserver_t *gfs, void* arg){
    gfs->arg = arg;
}

void gfserver_serve(gfserver_t *gfs){
    //creat server socket
    int servSock = socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    int opt = setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

    struct sockaddr_in servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(gfs->port);
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(servSock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0)
    {
        fprintf(stderr, "error in binding the server socket to local host port.\n");
        exit(1);
    }
    if(listen(servSock, gfs->maxnpending) < 0)
    {
        fprintf(stderr, "error in listenning.\n");
        exit(1);
    }

    while(1)
    {
        char* clientHeader = malloc(500);
        int bufSize = 200;
        
        struct sockaddr_storage sockTmp;
        socklen_t sockLen = sizeof(sockTmp);
        gfcontext_t gfc;
        gfc.sock = accept(servSock, &sockTmp, &sockLen);
        if(gfc.sock < 0)
            continue;
        struct timeval tv = {2, 0};
        opt = setsockopt(gfc.sock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*)&tv, sizeof(struct timeval));
 
        ssize_t byteReceived = 0, byteReceivedTotal = 0;
        printf("this is test1.\n");
        //receive the header information
        byteReceived = recv(gfc.sock, clientHeader + byteReceivedTotal, bufSize, 0);
        byteReceivedTotal = byteReceived;
        
        clientHeader[byteReceivedTotal] = '\0';
        printf("this is test2.%d\n", byteReceivedTotal);

        //compare the first 13 byte data in header to see whether it is correct 
        if(strncmp(clientHeader, "GETFILE GET /", 13) != 0)
        {
            gfs_sendheader(&gfc, GF_FILE_NOT_FOUND, 0);
            free(clientHeader);
            continue;
        }

        char* pathHead = strchr(clientHeader, '/');
        char* pathTail = strchr(pathHead, ' '); //pathTail can be ' ' or "\r\n\r\n"
        if(pathTail == NULL)
        {
            pathTail = strstr(clientHeader, "\r\n\r\n");
            if(pathTail == NULL || strcmp(pathTail, "\r\n\r\n") != 0) //if ther is no maker or not correct marker, malformed header
            {
                gfs_sendheader(&gfc, GF_FILE_NOT_FOUND, 0);
                free(clientHeader);
                continue;
            }
        }

        else if(*(pathTail+1) != '\0')  //pathtail should followed by '\0'
        {
            gfs_sendheader(&gfc, GF_FILE_NOT_FOUND, 0);
            free(clientHeader);
            continue;
        }

        char* path = malloc(pathTail - pathHead + 1);
        if(path ==  NULL)
        {
            fprintf(stderr, "error in allocating memory for path.\n");
            gfs_sendheader(&gfc, GF_FILE_NOT_FOUND, 0);
            free(clientHeader);
            continue;
        }

        int i = 0;
        //copy the path information to path
        while(pathHead != pathTail)
        {
            path[i++] = *pathHead++;
        }
        path[i] = '\0';
       
        gfs->handler(&gfc, path, gfs->arg);
        free(path);
        free(clientHeader);
        close(gfc.sock);
    }   
}

