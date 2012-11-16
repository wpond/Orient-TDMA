#include "system.h"

void QUEUE_Init(queue_t *queue, uint8_t *mem, uint16_t item_size, uint16_t queue_size)
{
	
	queue->queue = mem;
	queue->queue_size = queue_size;
	queue->item_size = item_size;
	queue->read_position = queue_size-1;
	queue->write_position = 0;
	
}

bool QUEUE_Read(queue_t *queue, uint8_t *payload)
{
	
	if (queue->queue_size == 0)
		return false;
	
	if ((queue->read_position + 1) % queue->queue_size == queue->write_position)
		return false;
	
	queue->read_position = (queue->read_position + 1) % queue->queue_size;
	memcpy(payload, &queue->queue[queue->read_position * queue->item_size], queue->item_size);
	
	return true;
	
}

bool QUEUE_Write(queue_t *queue, uint8_t *payload)
{
	
	if (queue->queue_size == 0)
		return false;
	
	if (queue->read_position == queue->write_position)
		return false;
	
	memcpy(&queue->queue[queue->write_position * queue->item_size],payload,queue->item_size);
	queue->write_position = (queue->write_position + 1) % queue->queue_size;
	
	return true;
	
}
