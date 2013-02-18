#ifndef __TRANSPORT_H__

	#define __TRANSPORT_H__
	
	#include <stdint.h>
	#include <stdbool.h>
	
	#include "packets.h"
	
	#define TRANSPORT_QUEUE_SIZE 128
	
	void TRANSPORT_Init();
	bool TRANSPORT_Send(uint8_t *data, uint8_t len);
	void TRANSPORT_Recv(PACKET_Raw *packet);
	void TRANSPORT_Reset();
	void TRANSPORT_Reload();
	void TRANSPORT_ReloadReady();
	
#endif