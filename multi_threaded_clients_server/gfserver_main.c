#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "gfserver.h"
#include "content.h"
#include "steque.h"
#include <pthread.h>
#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webproxy [options]\n"                                                      \
"options:\n"                                                                  \
"  -p                  Listen port (Default: 8888)\n"                         \
"  -c                  Content file mapping keys to content files\n"          \
"  -h                  Show this help message\n"                              


extern ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg);


/*
I use a queue of type steque_t to store job information: everytime when handler
function is called, boss thread will push the ctx and path into the queue(mutex
lock will be used during push operations), the worker thread will pop a pair of 
ctx and path from the queue, if the queue is empty, the worker thread will wait
until be signalled that queue is not empty.
*/

extern steque_t* queue;
extern pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
extern pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
extern void* worker(void* arg);
extern void create_threads(int num_thread);



/* Main ========================================================= */
int main(int argc, char **argv) {
  int option_char = 0;
  unsigned short port = 8888;
  char *content = "content.txt";
  gfserver_t *gfs;
  unsigned int num_threads = 1;

  // Parse and set command line arguments
  while ((option_char = getopt(argc, argv, "p:t:s:h")) != -1) {
    switch (option_char) {
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 't':
        num_threads = atoi(optarg);
        break;
      case 'c': // file-path
        content = optarg;
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
  //initialize the queue and create the threads
  queue = (steque_t*)malloc(sizeof(*queue));
  steque_init(queue);
  create_threads(num_threads);
  
  content_init(content);

  /*Initializing server*/
  gfs = gfserver_create();

  /*Setting options*/
  gfserver_set_port(gfs, port);
  gfserver_set_maxpending(gfs, 100);
  gfserver_set_handler(gfs, handler_get);
  gfserver_set_handlerarg(gfs, NULL);

  /*Loops forever*/
  gfserver_serve(gfs);
}