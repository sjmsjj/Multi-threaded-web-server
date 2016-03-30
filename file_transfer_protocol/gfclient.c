#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h> 
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

#include "gfclient.h"

typedef struct gfcrequest_t
{
    char* server;
    char* path;
    unsigned int port;
    gfstatus_t status;
    void (*headerfunc)(void*, size_t, void *);
    void* headerarg;
    void (*writefunc)(void*, size_t, void *);
    void* writearg;
    size_t filelen;
    size_t byteReceivedTotal;
}gfcrequest_t;


gfcrequest_t *gfc_create(){
	gfcrequest_t* gfc = malloc(sizeof(*gfc));
	if(gfc == NULL)
	{
		fprintf(stderr, "error in creating client instance.\n");
		exit(1);
	}
	gfc->filelen = 0;
    gfc->byteReceivedTotal = 0;
    gfc->status = 0;
    return gfc;
}

void gfc_set_server(gfcrequest_t *gfr, char* server){
	gfr->server = server;
}

void gfc_set_path(gfcrequest_t *gfr, char* path){
	gfr->path = path;
}

void gfc_set_port(gfcrequest_t *gfr, unsigned short port){
	gfr->port = port;
}

void gfc_set_headerfunc(gfcrequest_t *gfr, void (*headerfunc)(void*, size_t, void *)){
	gfr->headerfunc = headerfunc;
}

void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg){
    gfr->headerarg = headerarg;
}

void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *)){
    gfr->writefunc = writefunc;
}

void gfc_set_writearg(gfcrequest_t *gfr, void *writearg){
    gfr->writearg = writearg;
}

int gfc_perform(gfcrequest_t *gfr){
	//create client sockete
    int clientSock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(clientSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    struct hostent *he = gethostbyname(gfr->server);
    unsigned long serverAddr = *(unsigned long*)(he->h_addr_list[0]);
    
    struct sockaddr_in servSock;
    memset(&servSock, 0, sizeof(servSock));
    servSock.sin_family = AF_INET;
    servSock.sin_port = htons(gfr->port);
    servSock.sin_addr.s_addr = serverAddr;
    
    connect(clientSock, (struct sockaddr*)&servSock, sizeof(servSock));
	int returncode = 0;

	struct timeval tv = {2, 0};
    opt = setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*)&tv, sizeof(struct timeval));
 

	char clientHeader[200];  //for storing client header information
	snprintf(clientHeader, 199, "%s %s%s", "GETFILE GET", gfr->path, "\r\n\r\n");

	ssize_t byteSent, byteSentTotal = 0;
	//sending the header information
	while(byteSentTotal < strlen(clientHeader))
	{
		if((byteSent = write(clientSock, clientHeader+byteSentTotal, strlen(clientHeader)-byteSentTotal)) < 0)
		{
			close(clientSock);
			fprintf(stderr, "error in sending header message to server.\n");
			return -1;
		}
		byteSentTotal += byteSent;
	}



	int bufSize = 2048;
	int f1 = 0;  //f1 is to indicate whether or not the server header information is handled
	int fileLength = 0; //filelength is to store the str length of filelenth in the header information, namely strlen(filelength)
	char buffer[bufSize], temp[bufSize];//buffer for receiving information, temp is larger enough to store all header information (not necessarily all content information)
	memset(buffer, '\0', bufSize);
	memset(temp, '\0', bufSize);
	char *endMarker; //pointer to locate the location of "\r\n\r\n" in the header.
	ssize_t byteReceived;
	char* ok = "GETFILE OK";
	char* missing = "GETFILE FILE_NOT_FOUND";
	char* error = "GETFILE ERROR";
	while((byteReceived = recv(clientSock, buffer, bufSize-1, 0)) > 0)
	{
		gfr->byteReceivedTotal += byteReceived;
		if(gfr->byteReceivedTotal < bufSize)
		{
			strncpy(temp+gfr->byteReceivedTotal-byteReceived, buffer, byteReceived);
			//store the first bufSize header information
		}
		//if missing, return
        if(strncmp(temp, missing, strlen(missing)) == 0)
	    {
	    	gfr->status = GF_FILE_NOT_FOUND;
	    	fprintf(stderr, "%s\n", missing);
	    	return 0;
	    }

	    //if error, return
	    if(strncmp(temp, error, strlen(error)) == 0)
	    {
	    	gfr->status = GF_ERROR;
	    	fprintf(stderr, "%s\n", error);
	    	return 0;
	    }
        
        //the header is OK or malformed if the received byte is longer than length of missing
        //if the byte received so far is between OK and missing, the header can still be any status 
		if(gfr->byteReceivedTotal >= strlen(ok) && f1==0)
		{
			if(strncmp(temp, ok, strlen(ok)) != 0)
			{
				if(gfr->byteReceivedTotal >= strlen(missing))
					break;
				continue; 
				//if not OK, possible missing, error, or malformed, since the byte length received
				//so far can be between len(ok) and len(missing), continue recv until enough bytes
				//are received to make judgement. (missing is longest among the three)
				//missing means file not found.
			}
			else
			{
				//header can only be ok or malformed
				endMarker = strstr(temp, "\r\n\r\n");
				if(endMarker == NULL)
				{
					if(gfr->byteReceivedTotal > 50)
						break;
					continue;
					//if endMarker is not showup in the header, perhaps it has not been received yet.
					//if reasonably large byte (I use 50 here) has been received and there is still no
					//endmarker, then the header is malformed. 
				}


                //make sure the character in  the filelength is numerical digit.
				char* digit;
				for(digit = temp+11; digit != endMarker; digit++)
				{
					if(!isdigit(*digit))
						break;
				}
				//everything looks good, the header is ok
				*endMarker = '\0';
				gfr->filelen = atoi(temp+11); //store the filelength
				*endMarker = '\r';
				gfr->writefunc(endMarker+4, gfr->byteReceivedTotal-(endMarker+4-temp), gfr->writearg);

                fileLength = endMarker - (temp + 11); //calculate the string length of filelength, namely how many number of digits
                //gfr->byteReceivedTotal -= (fileLength + 15);
				f1 = 1;//signal header information has been successfully parsed and this part of job is done.
				continue;
			}
		}
		if(f1)
		{
			gfr->writefunc(buffer, byteReceived, gfr->writearg);  //keeping receiving and writing
		}
	}
	close(clientSock);
    //perror(temp);

	if(gfr->byteReceivedTotal == (gfr->filelen + fileLength+15))  //check whether all the contents have been received
	{
        gfr->byteReceivedTotal -= (fileLength + 15);
        gfr->status = GF_OK;
		return 0;
	}
    else
    {
        if(strncmp(temp, ok, strlen(ok)) == 0)  //status is ok but not all contents have been received
	    {
	    	gfr->status = GF_OK;
	    }
        else
    	    gfr->status = GF_INVALID; //malfored header information.
    	fprintf(stderr, "invalid header information.\n");
    	return -1;
    }
}

gfstatus_t gfc_get_status(gfcrequest_t *gfr){
	return gfr->status;
}

char* gfc_strstatus(gfstatus_t status){
	switch(status)
	{
		case GF_OK: return "GF_OK";
		case GF_FILE_NOT_FOUND: return "GF_FILE_NOT_FOUND";
		case GF_ERROR: return "GF_ERROR";
		case GF_INVALID: return "GF_INVALID";
		default: exit(1);
	}
}

size_t gfc_get_filelen(gfcrequest_t *gfr){
	return gfr->filelen;
}

size_t gfc_get_bytesreceived(gfcrequest_t *gfr){
	return gfr->byteReceivedTotal;

}

void gfc_cleanup(gfcrequest_t *gfr){
	free(gfr);
}

void gfc_global_init(){
}

void gfc_global_cleanup(){
}
