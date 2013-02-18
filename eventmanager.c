#include "eventmanager.h"

#include "queue.h"

/* variables */
queue_t eventQueue;
uint8_t eventQueueMem[EVENT_QUEUE_SIZE];

/* functions */
void EVENT_Init()
{
	QUEUE_Init(&eventQueue,eventQueueMem,1,EVENT_QUEUE_SIZE);
}

void EVENT_Set(uint8_t event)
{
	QUEUE_Queue(&eventQueue,&event);
}

uint8_t EVENT_Next()
{
	uint8_t ret;
	if (QUEUE_Dequeue(&eventQueue,&ret))
		return ret;
	else
		return 0xFF;
}

uint8_t EVENT_Count()
{
	return QUEUE_Count(&eventQueue);
}
