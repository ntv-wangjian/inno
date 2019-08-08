#ifndef NTV_QUEUE_H
#define NTV_QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

typedef struct ntv_queue_item
{
	char* filename;
	char* baseurl;
	double duration;                //from version 3, this means segmenet size
	int64_t size; 
	int64_t pos;
	int   insert_discont;
	int   byterange_mode;
    int   round_duration;

	struct ntv_queue_item* next;
} ntv_queue_item;

typedef struct ntv_queue
{
	struct ntv_queue_item* head;
	struct ntv_queue_item* tail;
	long   queue_size;                  //queue size.
} ntv_queue;

static void init_queue(ntv_queue* queue);
static int add_queue_item(ntv_queue* queue,ntv_queue_item* item);
static void free_queue(ntv_queue* queue);
static void free_next_item(ntv_queue_item* item);
static void free_queue_item(ntv_queue_item* item);
static int remove_queue_head(ntv_queue* queue);
static ntv_queue_item* new_queue_item(const char* filename,const char* baseurl,
	double duration,int64_t size,int64_t pos,int insert_discont,int  byterange_mode,int round_duration);

static void init_queue(ntv_queue* queue)
{
	queue->queue_size= 0;
	queue->head      = NULL;
	queue->tail      = NULL;
}

static int add_queue_item(ntv_queue* queue,ntv_queue_item* item)
{
	if(queue->queue_size == 0)
	{
		queue->head  = item;
	}
	else
	{
		queue->tail->next = item;
	}

	queue->tail = item;
	queue->queue_size++;

	return queue->queue_size;
}

static void free_queue(ntv_queue* queue){
	if(queue){
		ntv_queue_item* item = queue->head;
		if(item) free_next_item(item);
	}
}

static void free_next_item(ntv_queue_item* item)
{
	if(item->next)
	{
		free_next_item(item->next);
	}
	free_queue_item(item);
}

static void free_queue_item(ntv_queue_item* item)
{
	if(item->filename) free(item->filename);
	if(item->baseurl) free(item->baseurl);
	free(item);
}

static int remove_queue_head(ntv_queue* queue){
	if(queue){
		if( queue->head ){
			ntv_queue_item* item = queue->head;
			queue->head = item->next;
			free_queue_item(item);
			queue->queue_size--;
		}
		return queue->queue_size;
	}
	return 0;
}

static ntv_queue_item* new_queue_item(const char* filename,const char* baseurl,
	double duration,int64_t size,int64_t pos,int insert_discont,int  byterange_mode,int round_duration)
{
	ntv_queue_item* item= (ntv_queue_item*) malloc(sizeof(ntv_queue_item));
	item->filename   = NULL;
	item->baseurl    = NULL;
	item->next 		 = NULL;
	
	if(filename){
		item->filename = (char*)malloc(strlen(filename)+1);
		strcpy(item->filename,filename);
	}

	if(baseurl){
		item->baseurl = (char*)malloc(strlen(baseurl)+1);
		strcpy(item->baseurl,baseurl);
	}

	item->duration = duration;
	item->size     = size;
	item->pos      = pos;
	item->insert_discont = insert_discont;
	item->round_duration = round_duration;
	item->byterange_mode = byterange_mode;

	return item;
}


#endif