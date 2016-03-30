#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"
#include "content.h"
#include "steque.h"
#include <pthread.h>

#define BUFFER_SIZE 4096

void *worker(void* arg);
steque_t* queue;
pthread_mutex_t lock;
pthread_cond_t empty;

//function to create the worker threads
void create_threads(unsigned int num_threads)
{
	pthread_t workers[num_threads];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	int i;
	for(i = 0; i < num_threads; i++)
		pthread_create(&workers[i], &attr, worker, NULL);
    pthread_attr_destroy(&attr);
	for(i = 0; i < num_threads; i++)
		pthread_join(workers[i], NULL);
}


ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg){
	//push the ctx and path into the queue
	pthread_mutex_lock(&lock);
	steque_enqueue(queue, ctx);
	steque_enqueue(queue, path);
	pthread_mutex_unlock(&lock);
	pthread_cond_broadcast(&empty);
}

void *worker(void* arg)
{
    int fildes;
	ssize_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	char buffer[BUFFER_SIZE];
	gfcontext_t* ctx;
	char* path;

	while(1)
	{
		pthread_mutex_lock(&lock);
		while(queue->N == 0)
			pthread_cond_wait(&empty, &lock);   //wait until queue is not empty
		ctx = steque_pop(queue);
		path = steque_pop(queue);
		pthread_mutex_unlock(&lock);

		if( 0 > (fildes = content_get(path)))
		gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);

		/* Calculating the file size */
		file_len = lseek(fildes, 0, SEEK_END);

		gfs_sendheader(ctx, GF_OK, file_len);

		/* Sending the file contents chunk by chunk. */
		bytes_transferred = 0;
		while(bytes_transferred < file_len)
		{
			read_len = pread(fildes, buffer, BUFFER_SIZE, bytes_transferred);
			if (read_len <= 0)
			{
				fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
                break;
			}
			write_len = gfs_send(ctx, buffer, read_len);
			if (write_len != read_len)
			{
				fprintf(stderr, "handle_with_file write error");
			    break;
			}
			bytes_transferred += write_len;
	    }
        gfs_abort(ctx);

	}
}
