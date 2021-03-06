#ifndef __QUEUE_H__
	
	#define __QUEUE_H__
	
	#include <stdbool.h>
	#include <stdint.h>
	
	typedef struct
	{
		uint8_t *queue;
		uint16_t item_size, 
			item_count,
			start,
			end,
			count;
	}
	queue_t;
	
	void QUEUE_Init(queue_t *queue, uint8_t *mem, uint16_t item_size, uint16_t item_count);
	bool QUEUE_Queue(queue_t *queue, uint8_t *item);
	bool QUEUE_Dequeue(queue_t *queue, uint8_t *item);
	uint8_t* QUEUE_Peek(queue_t *queue, bool complete);
	uint8_t* QUEUE_Get(queue_t *queue, uint16_t offset);
	uint8_t* QUEUE_Next(queue_t *queue, bool complete);
	bool QUEUE_IsEmpty(queue_t *queue);
	bool QUEUE_IsFull(queue_t *queue);
	uint16_t QUEUE_Count(queue_t *queue);
	void QUEUE_Empty(queue_t *queue);
	
#endif