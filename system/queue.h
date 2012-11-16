#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
	
	uint8_t **queue;
	uint16_t	queue_size,
		item_size,
		read_position,
		write_position;
	
} queue_t;

void QUEUE_Init(queue_t *queue, uint8_t **mem, uint16_t item_size, uint16_t queue_size);
bool QUEUE_Read(queue_t *queue, uint8_t *payload);
bool QUEUE_Write(queue_t *queue, uint8_t *payload);

#endif