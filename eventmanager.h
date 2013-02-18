#ifndef __EVENTMANAGER_H__
	
	#define __EVENTMANAGER_H__
	
	#include <stdint.h>
	
	#define EVENT_QUEUE_SIZE 32
	
	typedef enum
	{
		EVENT_START_DATA = 0x01,
		EVENT_STOP_DATA = 0x02,
		EVENT_TRANSPORT_RELOAD = 0x03,
		
		EVENT_BASESTATION_SLOT = 0xFF,
	}
	EVENT_Type;
	
	void EVENT_Init();
	void EVENT_Set(uint8_t event);
	uint8_t EVENT_Next();
	uint8_t EVENT_Count();
	
#endif