#include "queue.h"

#include <stdint.h>
#include <string.h>

void QUEUE_Init(queue_t *queue, uint8_t *mem, uint8_t item_size, uint8_t item_count)
{
	
	queue->queue = mem;
	queue->item_size = item_size;
	queue->item_count = item_count;
	queue->start = 0;
	queue->end = 0;
	queue->count = 0;
	
}

bool QUEUE_Queue(queue_t *queue, uint8_t *item)
{
	
	if (QUEUE_IsFull(queue))
		return false;
	
	memcpy((void*)&queue->queue[queue->end * queue->item_size], (void*)item, queue->item_size);
	queue->end = (queue->end + 1) % queue->item_count;
	queue->count++;
	
	return true;
	
}

bool QUEUE_Dequeue(queue_t *queue, uint8_t *item)
{
	
	if (QUEUE_IsEmpty(queue))
		return false;
	
	memcpy((void*)item, (void*)&queue->queue[queue->start * queue->item_size], queue->item_size);
	queue->start = (queue->start + 1) % queue->item_count;
	queue->count--;
	
	return true;
	
}

uint8_t* QUEUE_Peek(queue_t *queue, bool complete)
{
	
	if (QUEUE_IsEmpty(queue))
		return NULL;
	
	uint8_t* item = &queue->queue[queue->start * queue->item_size];
	
	if (complete)
	{
		queue->start = (queue->start + 1) % queue->item_count;
		queue->count--;
	}
	
	return item;
	
}

uint8_t* QUEUE_Get(queue_t *queue, uint8_t offset)
{
	
	if (offset >= queue->count || QUEUE_IsEmpty(queue))
		return NULL;
	
	uint8_t pos = (queue->start + offset) % queue->item_count;
	
	uint8_t* item = &queue->queue[pos * queue->item_size];
	
	return item;
	
}

uint8_t* QUEUE_Next(queue_t *queue, bool complete)
{
	
	if (QUEUE_IsFull(queue))
		return NULL;
	
	uint8_t* item = &queue->queue[queue->end * queue->item_size];
	
	if (complete)
	{
		queue->end = (queue->end + 1) % queue->item_count;
		queue->count++;
	}
	
	return item;
	
}

bool QUEUE_IsEmpty(queue_t *queue)
{
	return queue->count == 0;
}

bool QUEUE_IsFull(queue_t *queue)
{
	return queue->count == queue->item_count;
}

uint8_t QUEUE_Count(queue_t *queue)
{
	return queue->count;
}

void QUEUE_Empty(queue_t *queue)
{
	
	queue->start = 0;
	queue->end = 0;
	queue->count = 0;
	
}