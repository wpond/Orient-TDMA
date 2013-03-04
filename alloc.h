#ifndef __ALLOC_H__

	#define __ALLOC_H__

	#include <stdint.h>
	#include <stdbool.h>
	
	#include "packets.h"
	
	#define ALLOC_LEASE_DEFAULT 10
	
	typedef struct
	{
		uint8_t nodeId;
		uint8_t lease;
		uint8_t ack;
	}
	ALLOC_Slot;
	
	void ALLOC_Init(uint8_t _slotOffset, uint8_t _slotCount);
	bool ALLOC_Request(uint8_t id);
	bool ALLOC_CheckNode(uint8_t nodeId, uint8_t *_slotId, uint8_t *_len, uint8_t *_lease, uint8_t *_ack);
	void ALLOC_RecvAck(PACKET_Raw *packet);
	void ALLOC_DecrementLeases();
	
#endif