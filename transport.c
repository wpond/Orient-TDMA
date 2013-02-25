#include "transport.h"

#include "efm32_int.h"
#include "efm32_timer.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "packets.h"
#include "radio.h"
#include "queue.h"
#include "tdma.h"

/* variables */
uint8_t windowFrameStart = 0,
	windowSegmentStart = 0;
uint8_t currentFrame = 0;
bool bufferFull = false,
	nextSegmentEnd = false,
	pendingReload = false,
	loaded = false,
	firstSend = true;

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
	
	#ifndef BASESTATION
		PACKET_Raw pRaw;
		PACKET_TransportData *pTrans;
		uint8_t segId = 0;
		
		if (len / 25 > TRANSPORT_QUEUE_SIZE - QUEUE_Count(&transportQueue) - 1)
			return false;
		
		// try to store in buffer
		while (len > 0)
		{
			
			pRaw.addr = 0x00;
			pRaw.type = PACKET_TRANSPORT_DATA;
			pTrans = (PACKET_TransportData*)pRaw.payload;
			
			pTrans->senderId = NODE_ID;
			pTrans->frameId = currentFrame;
			pTrans->segmentId = segId++;
			
			if (len <= 25)
			{
				pTrans->segmentFill = len;
				pTrans->flags = TRANSPORT_FLAG_SEGMENT_END;
				memcpy(pTrans->data,data,len);
				data += len;
				len = 0;
			}
			else
			{
				pTrans->segmentFill = 25;
				pTrans->flags = 0;
				memcpy(pTrans->data,data,25);
				data += 25;
				len -= 25;
			}
			
			if (bufferFull)
				pTrans->flags |= TRANSPORT_FLAG_BUFFER_FULL;
			
			if (QUEUE_IsEmpty(&transportQueue) && segId == 1)
				nextSegmentEnd = true;
			
			INT_Disable();
			bool result = QUEUE_Queue(&transportQueue,(uint8_t*)&pRaw);
			INT_Enable();
			
			if (!result)
			{
				bufferFull = true;
				pTrans->flags |= TRANSPORT_FLAG_BUFFER_FULL;
				
				// store in alternate buffer
			}
			
		}
		
		currentFrame = (currentFrame + 1) % 255;
		
		// if not, store on SD card
		if (firstSend)
		{
			pendingReload = true;
			TRANSPORT_Reload();
			firstSend = false;
		}
		
		return true;
	#else
		return false;
	#endif
	
}

void TRANSPORT_Recv(PACKET_Raw *packet)
{
	#ifndef BASESTATION
		PACKET_TransportAck *ackPacket = NULL;
		
		switch (packet->type)
		{
		case PACKET_TRANSPORT_ACK:
			
			ackPacket = (PACKET_TransportAck*)packet->payload;
			windowFrameStart = ackPacket->lastFrameId;
			windowSegmentStart = ackPacket->lastSegmentId;
			
			if (ackPacket->secondSlot)
			{
				TDMA_SecondSlot slot;
				slot.slot = ackPacket->slotId;
				slot.len = ackPacket->len;
				slot.lease = ackPacket->lease;
				slot.enabled = true;
				
				TDMA_ConfigureSecondSlot(&slot);
				
				char tmsg[255];
				sprintf(tmsg,"%i: Second slot received [id: %i; len: %i; lease: %i]\n",(int)TIMER_CounterGet(TIMER1),(int)slot.slot,(int)slot.len,(int)slot.lease);
				TRACE(tmsg);
				
				PACKET_Raw pRaw;
				pRaw.addr = 0;
				pRaw.type = PACKET_TDMA_ACK;
				pRaw.payload[0] = ackPacket->seqNum;
				
				RADIO_Send((uint8_t*)&pRaw);
			}
			
			char tmsg[255];
			sprintf(tmsg,"%i: Transport ack %i/%i received (seg/frame)\n",(int)TIMER_CounterGet(TIMER1),(int)ackPacket->lastSegmentId,(int)ackPacket->lastFrameId);
			TRACE(tmsg);
			
			break;
		case PACKET_TRANSPORT_DATA:
			break;
		default:
			break;
		}
		
		PACKET_Raw *packetPtr = NULL;
		PACKET_TransportData *pData = NULL;
		uint8_t expFrame = windowFrameStart, expSeg = (windowSegmentStart + 1) % 0xFF;
		
		if (expSeg == 0)
		{
			expFrame = (windowFrameStart + 1) % 0xFF;
		}
		
		char tmsg[255];
		sprintf(tmsg,"%i: Transport %i/%i expected next (seg/frame)\n",(int)TIMER_CounterGet(TIMER1),(int)expSeg,(int)expFrame);
		TRACE(tmsg);
		
		packetPtr = (PACKET_Raw*)QUEUE_Peek(&transportQueue,false);
		
		while (packetPtr != NULL)
		{
			pData = (PACKET_TransportData*)packetPtr->payload;
			if ((!(nextSegmentEnd) && pData->frameId == expFrame && pData->segmentId == expSeg) ||
				((nextSegmentEnd) && (pData->frameId == ((expFrame + 1) % 255)) && pData->segmentId == 0))
			{
				break;
			}
			nextSegmentEnd = pData->flags & TRANSPORT_FLAG_SEGMENT_END;
			packetPtr = (PACKET_Raw*)QUEUE_Peek(&transportQueue,true);
			if (packetPtr == NULL)
				break;
			char tmsg[255];
			sprintf(tmsg,"%i: Transport %i/%i sent successfully (seg/frame)\n",(int)TIMER_CounterGet(TIMER1),(int)((PACKET_TransportData*)packetPtr->payload)->segmentId,(int)((PACKET_TransportData*)packetPtr->payload)->frameId);
			TRACE(tmsg);
			
			packetPtr = (PACKET_Raw*)QUEUE_Peek(&transportQueue,false);
		}
		
		
		
		packetPtr = (PACKET_Raw*)QUEUE_Peek(&transportQueue,false);
		
		if (packetPtr != NULL)
		{
			char tmsg[255];
			sprintf(tmsg,"%i: Transport %i/%i next to send [window start: %i/%i] (seg/frame)\n",(int)TIMER_CounterGet(TIMER1),(int)((PACKET_TransportData*)packetPtr->payload)->segmentId,(int)((PACKET_TransportData*)packetPtr->payload)->frameId,(int)windowSegmentStart,(int)windowFrameStart);
			TRACE(tmsg);
		}
		
		pendingReload = true;
	#endif
}

void TRANSPORT_Reload()
{
	
	#ifndef BASESTATION
		if (!pendingReload || loaded)
			return;
		
		PACKET_Raw *packetPtr = NULL;
			
		// check for reload of buffer from SD card
		
		// reload physical buffer
		uint8_t i = 0;
		do
		{
			packetPtr = (PACKET_Raw*)QUEUE_Get(&transportQueue,i++);
		}
		while (packetPtr != NULL && RADIO_Send((uint8_t*)packetPtr));
		
		
		char tmsg[255];
		sprintf(tmsg,"%i: Transport %i reloaded...%i/%i (seg/frame)\n",(int)TIMER_CounterGet(TIMER1),(int)i-1,(int)windowSegmentStart,(int)windowFrameStart);
		TRACE(tmsg);
		
		pendingReload = false;
		loaded = true;
	#endif
}

void TRANSPORT_ReloadReady()
{
	pendingReload = true;
}

void TRANSPORT_Reset()
{
	loaded = false;
	pendingReload = false;
}
