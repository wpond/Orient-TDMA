#ifndef __PACKETS_H__
	
	#define __PACKETS_H__
	
	#include <stdint.h>
	
	typedef enum 
	{
		PACKET_TDMA_TIMING = 0x00,
		PACKET_HELLO = 0x01,
		PACKET_TDMA_CONFIG = 0x02,
		PACKET_TDMA_ENABLE = 0x03,
		PACKET_TDMA_SLOT = 0x04, // piggyback on transport ack
		PACKET_TDMA_ACK = 0x05,
		PACKET_TRANSPORT_DATA = 0x06,
		PACKET_TRANSPORT_ACK = 0x07,
		PACKET_EVENT = 0x08,
	}
	PACKET_Type;
	
	typedef struct
	{
		
		uint8_t addr;
		uint8_t type;
		uint8_t payload[30];
		
	}
	PACKET_Raw;
	
	typedef struct
	{
		
		uint8_t challengeResponse;
		uint8_t padding[29];
		
	}
	PACKET_PayloadHello;
	
	#define HELLO_CHALLENGE 0xFF
	
	typedef struct
	{
		
		uint8_t senderId;
		uint8_t frameId;
		uint8_t segmentId;
		uint8_t segmentFill;
		uint8_t flags;
		uint8_t data[25];
		
	}
	PACKET_TransportData;
	
	#define TRANSPORT_FLAG_SEGMENT_END 0x01 					// 0000 0001
	#define TRANSPORT_FLAG_BUFFER_LEVEL_HIGH 0x02				// 0000 0010
	#define TRANSPORT_FLAG_BUFFER_FULL 0x04 					// 0000 0100
	#define TRANSPORT_FLAG_EXTERNAL_STORAGE_FULL 0x08 			// 0000 1000
	#define TRANSPORT_FLAG_UNUSED_1 0x10						// 0001 0000
	#define TRANSPORT_FLAG_UNUSED_2 0x20						// 0010 0000
	#define TRANSPORT_FLAG_UNUSED_3 0x40						// 0100 0000
	#define TRANSPORT_FLAG_SLOT_REQUEST 0x80					// 1000 0000
	
	typedef struct
	{
		
		uint8_t lastFrameId;
		uint8_t lastSegmentId;
		bool secondSlot;
		uint8_t seqNum;
		uint8_t slotId;
		uint8_t len;
		uint8_t lease;
		uint8_t padding[23];
		
	}
	PACKET_TransportAck;
	
	typedef struct
	{
		uint8_t seqNum;
		uint8_t payload[28];
	}
	PACKET_TDMA;
	
	typedef struct
	{
		uint8_t seqNum;
		uint8_t slotId;
		uint8_t len;
		uint8_t lease;
		uint8_t padding[26];
	}
	PACKET_TDMASlot;
	
#endif