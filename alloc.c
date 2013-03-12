#include "alloc.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "packets.h"
#include "config.h"

/* variables */
static ALLOC_Slot allocTable[255];
static uint8_t slotCount,
	slotOffset,
	nextAck = 253;

/* prototypes */
static void ALLOC_ResetSlot(ALLOC_Slot* slot);
static uint8_t ALLOC_NextEmptySlot();
static uint8_t ALLOC_NodeSlot(uint8_t id);
static uint8_t ALLOC_NextAck();
static bool ALLOC_Alloc(uint8_t slotId, uint8_t id, uint8_t lease);
static void ALLOC_TraceTable();

/* functions */
static void ALLOC_TraceTable()
{
	static char tmsg[64];
	
	sprintf(tmsg,"\nALLOCATION TABLE\n");
	TRACE(tmsg);
	
	sprintf(tmsg,"\n| %-8s| %-8s| %-8s| %-8s|\n","Slot ID","Node ID","Lease","ACK");
	TRACE(tmsg);
	sprintf(tmsg,"-----------------------------------------\n");
	TRACE(tmsg);
	
	uint8_t i;
	for (i = 0; i < 255; i++)
	{
		if (allocTable[i].lease > 0)
		{
			sprintf(tmsg,"| %-8i| %-8i| %-8i| %-8i|\n",i,allocTable[i].nodeId,allocTable[i].lease,allocTable[i].ack);
			TRACE(tmsg);
		}
	}
}

void ALLOC_Init(uint8_t _slotOffset, uint8_t _slotCount)
{
	
	uint8_t i;
	for (i = _slotOffset; i < _slotOffset + _slotCount; i++)
	{
		ALLOC_ResetSlot(&allocTable[i]);
	}
	
	slotCount = _slotCount;
	slotOffset = _slotOffset;
	
}

static void ALLOC_ResetSlot(ALLOC_Slot* slot)
{
	slot->nodeId = 0;
	slot->lease = 0;
	slot->ack = 254;
}

static uint8_t ALLOC_NextEmptySlot()
{
	
	uint8_t i, 
		nextSlot = 0,
		diff = 2;
	ALLOC_Slot* slot;
	for (i = 0; i < slotCount; i++)
	{
		slot = &allocTable[nextSlot + slotOffset];
		if (slot->lease == 0)
		{
			return nextSlot + slotOffset;
		}
		
		nextSlot = (nextSlot + diff) % slotCount;
	}
	
	return 255;
	
}

static uint8_t ALLOC_NodeSlot(uint8_t id)
{
	
	uint8_t i;
	for (i = slotOffset; i < slotOffset + slotCount; i++)
	{
		if (allocTable[i].nodeId == id && allocTable[i].lease > 0)
			return i;
	}
	
	return 255;
	
}

static uint8_t ALLOC_NextAck()
{
	nextAck = (nextAck + 1) % 253;
	return nextAck;
}

static bool ALLOC_Alloc(uint8_t slotId, uint8_t id, uint8_t lease)
{
	if (slotId < slotOffset || slotId >= slotOffset+slotCount)
		return false;
	if (allocTable[slotId].lease > 0)
		return false;
	
	allocTable[slotId].nodeId = id;
	allocTable[slotId].lease = lease;
	allocTable[slotId].ack = 253; // setting it to 253 will ensure a new ack next iteration
	
	return true;
}

bool ALLOC_Request(uint8_t id)
{
	
	uint8_t slotId = ALLOC_NodeSlot(id);
	if (slotId == 255)
	{
		
		slotId = ALLOC_NextEmptySlot();
		
		if (slotId != 255)
		{
			return ALLOC_Alloc(slotId,id,ALLOC_LEASE_DEFAULT);
		}
		
	}
	else
	{
		
		uint8_t curId = slotId;
		while (true)
		{
			
			if (++curId >= slotOffset+slotCount)
				break;
			
			if (allocTable[curId].lease > 0)
			{
				if (allocTable[curId].nodeId == id)
					continue;
				else
					break;
			}
			else
			{
				return ALLOC_Alloc(curId,id,ALLOC_LEASE_DEFAULT);
			}
				
		}
		
		curId = slotId;
		while (true)
		{
			
			if (--curId < slotOffset)
				break;
			
			if (allocTable[curId].lease > 0)
			{
				if (allocTable[curId].nodeId == id)
					continue;
				else
					break;
			}
			else
			{
				return ALLOC_Alloc(curId,id,ALLOC_LEASE_DEFAULT);
			}
			
		}
		
	}
	
	return false;
	
}

bool ALLOC_CheckAndDecrement(uint8_t nodeId, uint8_t *_slotId, uint8_t *_len, uint8_t *_lease, uint8_t *_ack)
{
	
	uint8_t slotId = ALLOC_NodeSlot(nodeId);
	if (slotId == 255)
		return false;
	
	bool ackRequired = false;
	uint8_t len = 0,
		minLease = 255;
	while (true)
	{
		
		if (!(allocTable[slotId+len].lease > 0 && allocTable[slotId+len].nodeId == nodeId))
			break;
		
		if (allocTable[slotId+len].lease < minLease)
			minLease = allocTable[slotId+len].lease;
		
		allocTable[slotId+len].lease--;
		
		if (allocTable[slotId+len].ack != 254)
		{
			ackRequired = true;
			allocTable[slotId+len].ack = 254;
		}
		
		len++;
		
	}
	
	if (ackRequired)
	{
		allocTable[slotId].ack = ALLOC_NextAck();
		*_slotId = slotId;
		*_len = len;
		*_lease = minLease;
		*_ack = allocTable[slotId].ack;
		return true;
	}
	
	return false;
	
}
