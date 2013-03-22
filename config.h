#ifndef __CONFIG_H__

	#define __CONFIG_H__
	
	//#define BASESTATION
	
	
	#define BASESTATION_ID 0x00
	#define BROADCAST_ID 0xFF
	
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
			return;
			uint8_t len, msglen;
			bool end;
			static uint8_t packet[32];
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
		#else
			USB_Transmit((uint8_t*)msg,strlen(msg));
		#endif
	}
	
	void wait(uint32_t ms);
	
#endif