#include "transport.h"

#include "efm32_int.h"

#include "config.h"
#include "packets.h"
#include "radio.h"
#include "queue.h"

/* variables */
uint8_t windowFrameStart = 0,
	windowSegmentStart = 0;
uint8_t currentFrame = 0;

queue_t transportQueue;
uint8_t transportQueueMemory[TRANSPORT_QUEUE_SIZE * sizeof(PACKET_Raw)];

/* prototypes */

/* functions */
void TRANSPORT_Init()
{
	
	QUEUE_Init(&transportQueue, transportQueueMemory, sizeof(PACKET_Raw), TRANSPORT_QUEUE_SIZE);
	
}

bool TRANSPORT_Send(uint8_t *data, uint8_t len)
{
	
	// try to store in buffer
	while (len > 0)
	{
		
		
		
	}
	
	// if not, store on SD card
	
	
	return false;
	
}

void TRANSPORT_Reload()
{
	
	PACKET_Raw packet, *packetPtr;
	PACKET_TransportAck *ackPacket;
	PACKET_TransportData *dataPacket;
	
	// receive all packets
	while (RADIO_Recv((uint8_t*)&packet))
	{
		
		switch (packet.type)
		{
		case PACKET_TRANSPORT_ACK:
			
			ackPacket = (PACKET_TransportAck*)packet.payload;
			
			// update window start and end
			if (windowFrameStart <= ackPacket->lastFrameId)
			{
				
				windowFrameStart = ackPacket->lastFrameId;
				windowSegmentStart = ackPacket->lastSegmentId;
				
			}
			else if (windowSegmentStart < ackPacket->lastSegmentId)
			{
				
				windowSegmentStart = ackPacket->lastSegmentId;
				
			}
			
			break;
		case PACKET_TRANSPORT_DATA:
			
			// if base station, send over USB
			#ifdef BASESTATION
				
				USB_Transmit((uint8_t*)&packet, sizeof(PACKET_Raw));
				
			#endif
			
			break;
		default:
			break;
		}
		
	}
	
	// remove sucessful packets
	// disbale irqs to stop any packets being added to queue while we're using memory
	INT_Disable();
	do
	{
		
		packetPtr = (PACKET_Raw*)QUEUE_Peek(&transportQueue,true);
		dataPacket = (PACKET_TransportData*)packetPtr->payload;
		
	} while (packetPtr != NULL && 
		dataPacket->frameId != windowFrameStart && 
		dataPacket->segmentId != windowSegmentStart);
	
	INT_Enable();
	
	// check for reload of buffer from SD card
	
	// reload physical buffer
	uint8_t i = 0;
	do
	{
		packetPtr = (PACKET_Raw*)QUEUE_Get(&transportQueue,i++);
	}
	while (packetPtr != NULL && RADIO_Send((uint8_t*)packetPtr));
	
}
