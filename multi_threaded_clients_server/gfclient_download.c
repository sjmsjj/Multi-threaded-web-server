#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

#include "workload.h"
#include "gfclient.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -s [server_addr]    Server address (Default: 0.0.0.0)\n"                   \
"  -p [server_port]    Server port (Default: 8888)\n"                         \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \
"  -t [nthreads]       Number of threads (Default 1)\n"                       \
"  -n [num_requests]   Requests download per thread (Default: 1)\n"           \
"  -h                  Show this help message\n"                              \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"server",        required_argument,      NULL,           's'},
  {"port",          required_argument,      NULL,           'p'},
  {"workload-path", required_argument,      NULL,           'w'},
  {"nthreads",      required_argument,      NULL,           't'},
  {"nrequests",     required_argument,      NULL,           'n'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};







/*
 in the client, each worker thread starts its job with getting a new path by 
 calling the workload_get_path function, so the critical sention include the
 calling to the global functions like workload_get_path, localpath, openFile.
 A mutex lock is used on the calling on these functions in each worker thread.
*/

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
void* worker(void*);
char *server2;   //the worker function needs to access server and port
unsigned short port2;



static void Usage() {
  fprintf(stdout, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path){
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE* openFile(char *path){
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while(NULL != (cur = strchr(prev+1, '/'))){
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)){
      if (errno != EEXIST){
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if( NULL == (ans = fopen(&path[0], "w"))){
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void* data, size_t data_len, void *arg){
  FILE *file = (FILE*) arg;

  fwrite(data, 1, data_len, file);
}



/* Main ========================================================= */
int main(int argc, char **argv) {
/* COMMAND LINE OPTIONS ============================================= */
  char *server = "localhost";
  unsigned short port = 8888;
  char *workload_path = "workload.txt";

  int i;
  int option_char = 0;
  int nrequests = 1;
  int nthreads = 1;
  

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "s:p:w:n:t:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 's': // server
        server = optarg;
        break;
      case 'p': // port
        port = atoi(optarg);
        break;
      case 'w': // workload-path
        workload_path = optarg;
        break;
      case 'n': // nrequests
        nrequests = atoi(optarg);
        break;
      case 't': // nthreads
        nthreads = atoi(optarg);
        break;
      case 'h': // help
        Usage();
        exit(0);
        break;                      
      default:
        Usage();
        exit(1);
    }
  }
   
  if( EXIT_SUCCESS != workload_init(workload_path)){
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }
    server2 =server;
    port2 = port;
    
    gfc_global_init();
    //creat worker threads
    pthread_t workers[nthreads];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    for(i = 0; i < nthreads; i++)
      pthread_create(&workers[i], &attr, worker, &nrequests);
    pthread_attr_destroy(&attr);
    
    for(i = 0; i < nthreads; i++)
       pthread_join(workers[i], NULL);
  gfc_global_cleanup();

  return 0;
}  

void *worker(void* requests)
{
  int* nrequests = (int*)requests;
  gfcrequest_t *gfr;
  char local_path[256];;
  char* req_path;
  FILE* file;
  int j = 0;
  int returncode;
    
  while(j < *nrequests)
  {
      j++;
      //lock is applied on operations calling workload_get_path, localpath, openfile functions.
      pthread_mutex_lock(&lock);
      req_path = workload_get_path();
      if(strlen(req_path) > 256){
        fprintf(stderr, "Request path exceeded maximum of 256 characters\n.");
        continue;
      }   
      localPath(req_path, local_path);
      file = openFile(local_path);
      pthread_mutex_unlock(&lock);
      
      //the followig operations are thread safe.    
      gfr = gfc_create();
      gfc_set_server(gfr, server2);
      gfc_set_path(gfr, req_path);
      gfc_set_port(gfr, port2);
      gfc_set_writefunc(gfr, writecb);
      gfc_set_writearg(gfr, file);
   
      fprintf(stdout, "Requesting %s%s\n", server2, req_path);
      
      if ( 0 > (returncode = gfc_perform(gfr))){
        fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
        fclose(file);
        if ( 0 > unlink(local_path))
          fprintf(stderr, "unlink failed on %s\n", local_path);
    }
    else {
        fclose(file);
    }
    
    if ( gfc_get_status(gfr) != GF_OK){
      if ( 0 > unlink(local_path))
        fprintf(stderr, "unlink failed on %s\n", local_path);
    }

    fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(gfr)));
    fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr));
  }
}
