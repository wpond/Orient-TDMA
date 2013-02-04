#ifndef __PACKETS_H__
	
	#define __PACKETS_H__
	
	#include <stdint.h>
	
	typedef enum 
	{
		PACKET_TDMA_TIMING = 0x00,
		PACKET_HELLO = 0x01,
		PACKET_TDMA_CONFIG = 0x02,
		PACKET_TDMA_SLOT = 0x03,
		PACKET_TRANSPORT_DATA = 0x04,
		PACKET_TRANSPORT_ACK = 0x05,
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
		
		uint8_t frameId;
		uint8_t segmentId;
		uint8_t segmentFill;
		uint8_t flags;
		uint8_t data[26];
		
	}
	PACKET_TransportData;
	
	#define TRANSPORT_FLAG_BUFFER_FULL 0x00
	#define TRANSPORT_FLAG_SLOT_REQUEST 0x01
	#define TRANSPORT_FLAG_SD_IN_USE 0x02
	#define TRANSPORT_FLAG_SEGMENT_END 0x04
	
	typedef struct
	{
		
		uint8_t lastFrameId;
		uint8_t lastSegmentId;
		uint8_t padding[28];
		
	}
	PACKET_TransportAck;
	
#endif