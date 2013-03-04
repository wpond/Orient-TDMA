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
#include "alloc.h"

/* variables */
typedef struct
{
	bool init;
	uint8_t lastFrameId;
	uint8_t lastSegmentId;
	uint8_t lastFlags;
}
TRANSPORT_PacketTracker;

TRANSPORT_PacketTracker tracker[254];

uint8_t windowFrameStart = 0,
	windowSegmentStart = 0;
uint8_t currentFrame = 0;

bool bufferFull = false,
	nextSegmentEnd = false,
	pendingReload = false,
	firstSend = true;
uint16_t loadPosition = 0;

queue_t transportQueue;
uint8_t transportQueueMemory[TRANSPORT_QUEUE_SIZE * sizeof(PACKET_Raw)];

/* prototypes */
void TRANSPORT_SendAcks();

/* functions */
void TRANSPORT_Init()
{
	
	QUEUE_Init(&transportQueue, transportQueueMemory, sizeof(PACKET_Raw), TRANSPORT_QUEUE_SIZE);
	
	uint8_t i;
	for (i = 0; i < 254; i++)
	{
		tracker[i].init = false;
	}
	
}

bool TRANSPORT_Send(uint8_t *data, uint8_t len)
{
	
	#ifndef BASESTATION
		PACKET_Raw pRaw;
		PACKET_TransportData *pTrans;
		uint8_t segId = 0;
		
		// check space in buffers
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
			
			pTrans->flags |= TRANSPORT_FLAG_SLOT_REQUEST;
			
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
	{
		#ifdef BASESTATION
			
			PACKET_TransportData *dataPacket = (PACKET_TransportData*)packet->payload;
			
			char tmsg[255];
			sprintf(tmsg,"%i: RECV DATA %i/%i [node %i]\n",(int)TIMER_CounterGet(TIMER1),(int)dataPacket->segmentId,(int)dataPacket->frameId,(int)dataPacket->senderId);
			//TRACE(tmsg);
			
			if (tracker[dataPacket->senderId].init)
			{
			
				uint8_t nextFrameId = tracker[dataPacket->senderId].lastFrameId,
					nextSegId = tracker[dataPacket->senderId].lastSegmentId;
				
				if (tracker[dataPacket->senderId].lastFlags & TRANSPORT_FLAG_SEGMENT_END)
				{
					nextFrameId = (nextFrameId + 1) % 255;
					nextSegId = 0;
				}
				else
				{
					nextSegId++;
				}
				
				if (nextFrameId == dataPacket->frameId && nextSegId == dataPacket->segmentId)
				{
					tracker[dataPacket->senderId].lastFlags = dataPacket->flags;
					tracker[dataPacket->senderId].lastFrameId = dataPacket->frameId;
					tracker[dataPacket->senderId].lastSegmentId = dataPacket->segmentId;
				}
				else
				{
					sprintf(tmsg,"%i: out of sequence packet received %i/%i [node %i]\n",(int)TIMER_CounterGet(TIMER1),(int)dataPacket->segmentId,(int)dataPacket->frameId,(int)dataPacket->senderId);
					TRACE(tmsg);
				}
			
			}
			else
			{
				tracker[dataPacket->senderId].init = true;
				tracker[dataPacket->senderId].lastFlags = dataPacket->flags;
				tracker[dataPacket->senderId].lastFrameId = dataPacket->frameId;
				tracker[dataPacket->senderId].lastSegmentId = dataPacket->segmentId;
			}
			
		#endif
		
		break;
	}
	default:
		break;
	}
	
	#ifndef BASESTATION
	
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
			//TRACE(tmsg);
			
			packetPtr = (PACKET_Raw*)QUEUE_Peek(&transportQueue,false);
		}
		
		
		
		packetPtr = (PACKET_Raw*)QUEUE_Peek(&transportQueue,false);
		
		if (packetPtr != NULL)
		{
			char tmsg[255];
			sprintf(tmsg,"%i: Transport %i/%i next to send [window start: %i/%i] (seg/frame)\n",(int)TIMER_CounterGet(TIMER1),(int)((PACKET_TransportData*)packetPtr->payload)->segmentId,(int)((PACKET_TransportData*)packetPtr->payload)->frameId,(int)windowSegmentStart,(int)windowFrameStart);
			TRACE(tmsg);
		}
		
		loadPosition = 0;
		pendingReload = true;
	#endif
}

void TRANSPORT_Reload()
{
	
	#ifndef BASESTATION
		if (!pendingReload)
		{
			return;
		}
		
		PACKET_Raw *packetPtr = NULL;
		
		// check for reload of buffer from SD card
		
		// reload physical buffer
		//uint8_t i = 0;
		uint8_t firstSeg, firstFrame, lastSeg, lastFrame, loadPositionStart = loadPosition;
		
		packetPtr = (PACKET_Raw*)QUEUE_Get(&transportQueue,loadPosition);
		
		if (packetPtr != NULL)
		{
			firstSeg = ((PACKET_TransportData*)packetPtr->payload)->segmentId;
			firstFrame = ((PACKET_TransportData*)packetPtr->payload)->frameId;
			lastSeg = ((PACKET_TransportData*)packetPtr->payload)->segmentId;
			lastFrame = ((PACKET_TransportData*)packetPtr->payload)->frameId;
		}
		
		while (packetPtr != NULL && RADIO_Send((uint8_t*)packetPtr))
		{
			//packetPtr = (PACKET_Raw*)QUEUE_Get(&transportQueue,i++);
			packetPtr = (PACKET_Raw*)QUEUE_Get(&transportQueue,++loadPosition);
		}
		
		packetPtr = (PACKET_Raw*)QUEUE_Get(&transportQueue,loadPosition-1);
		lastSeg = ((PACKET_TransportData*)packetPtr->payload)->segmentId;
		lastFrame = ((PACKET_TransportData*)packetPtr->payload)->frameId;
		
		char tmsg[255];
		sprintf(tmsg,"%i: Transport %i reloaded...%i/%i to %i/%i (seg/frame)\n",(int)TIMER_CounterGet(TIMER1),(int)loadPosition-loadPositionStart,(int)firstSeg,(int)firstFrame,(int)lastSeg,(int)lastFrame);
		TRACE(tmsg);
		
		//loadPosition -= 1;
		
		pendingReload = false;
	#endif
}

void TRANSPORT_SendAcks()
{
	
	char tmsg[255];
	
	PACKET_Raw pRaw;
	PACKET_TransportAck *ackPacket;
	
	// IMPORTANT: second slot
	memset((void*)&pRaw,0,32);
	
	pRaw.type = PACKET_TRANSPORT_ACK;
	ackPacket = (PACKET_TransportAck*)pRaw.payload;
	
	uint8_t i;
	for (i = 0; i < 254; i++)
	{
		if (tracker[i].init)
		{
			pRaw.addr = i;
			ackPacket->lastFrameId = tracker[i].lastFrameId;
			ackPacket->lastSegmentId = tracker[i].lastSegmentId;
			
			if (tracker[i].lastFlags & TRANSPORT_FLAG_SLOT_REQUEST)
			{
				ALLOC_Request(i);
			}
			
			ackPacket->secondSlot = ALLOC_CheckNode(i,
										&ackPacket->slotId,
										&ackPacket->len,
										&ackPacket->lease,
										&ackPacket->seqNum);
			
			if (!RADIO_Send((uint8_t*)&pRaw))
				TRACE("<<<<< UNABLE TO SEND ACK PACKET >>>>>\n");
			
			sprintf(tmsg,"%i: SENT ACK %i/%i TO NODE %i\n",(int)TIMER_CounterGet(TIMER1),(int)tracker[i].lastSegmentId,(int)tracker[i].lastFrameId,(int)i);
			TRACE(tmsg);
		}
	}
	
}

void TRANSPORT_ReloadReady()
{
	TRACE("RELOAD READY\n");
	pendingReload = true;
}

void TRANSPORT_Reset()
{
	pendingReload = false;
	INT_Disable();
	QUEUE_Empty(&txQueue);
	INT_Enable();
	
	#ifdef BASESTATION
		
		ALLOC_DecrementLeases();
		TRANSPORT_SendAcks();
		
	#endif
}
