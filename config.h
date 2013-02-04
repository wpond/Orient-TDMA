#ifndef __CONFIG_H__
	
	#define __CONFIG_H__
	
	#define BASESTATION
	
	#ifdef BASESTATION
		#define NODE_ID 0
	#else
		#define NODE_ID 1
	#endif

	#define NODE_CHANNEL 102

	#include <string.h>

	#include "usb.h"

	static void inline TRACE(char* msg)
	{
		USB_Transmit((uint8_t*)msg,strlen(msg));
	}
	
	void wait(uint32_t ms);

#endif