#ifndef __CONFIG_H__
	
	#define __CONFIG_H__
	
	// controls whether basestation or not
	#define BASESTATION
	
	
	
	#define BASESTATION_ID 0
	
	#ifdef BASESTATION
		#define NODE_ID BASESTATION_ID
	#else
		#define NODE_ID 1
	#endif
	
	#define NODE_CHANNEL 2

	#include <stdint.h>
	#include <stdio.h>
	#include <stdbool.h>
	#include <string.h>
	
	#include "efm32_int.h"
	
	#include "usb.h"
	
	static void inline TRACE(char* msg)
	{
		#ifdef BASESTATION
			//INT_Disable();
			uint8_t len, msglen;
			bool end;
			uint8_t packet[32];
			msglen = strlen(msg);
			
			do
			{
				if (msglen > 28)
				{
					len = 28;
					end = false;
				}
				else
				{
					len = msglen;
					end = true;
				}
				packet[0] = 0;
				packet[1] = 0xFF;
				packet[2] = len;
				packet[3] = end;
				memcpy(&packet[4],msg,len);
				USB_Transmit((uint8_t*)packet,32);
				msg += len;
				msglen -= len;
			}
			while (msglen > 0);
			//INT_Enable();
		#else
			USB_Transmit((uint8_t*)msg,strlen(msg));
		#endif
	}
	
	static void inline TRACESTRUCT(void* s, uint8_t len)
	{
		char *c = (char*)s;
		int i;
		char msg[len*5 + 1];
		for (i = 0; i < len; i++)
		{
			sprintf(&msg[i*5],"0x%2.2X ",c[i]);
		}
		TRACE(msg);
	}
	
	void wait(uint32_t ms);

#endif