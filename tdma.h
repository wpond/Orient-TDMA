#ifndef __TDMA_H__
	
	#define __TDMA_H__
	
	#include <stdbool.h>
	#include <stdint.h>
	
	#include "packets.h"
	
	#define TDMA_INITIAL_SYNC_OFFSET 230
	
	typedef struct
	{
		bool master;
		uint8_t channel;
		uint8_t slot;
		uint8_t slotCount;
		uint32_t guardPeriod;
		uint32_t transmitPeriod;
		uint32_t protectionPeriod;
	}
	TDMA_Config;
	
	typedef struct
	{
		bool enabled;
		uint8_t slot;
		uint8_t len;
		uint8_t lease;
	}
	TDMA_SecondSlot;
	
	void TDMA_Init();
	void TDMA_Enable(bool enable);
	void TDMA_CheckSync();
	
	bool TDMA_IsTimingPacket(uint8_t packet[32]);
	void TDMA_TimingPacketReceived();
	void TDMA_RadioTransfer(uint8_t data[33]);
	
	bool TDMA_PacketConfigure(PACKET_Raw *packet);
	bool TDMA_PacketEnable(PACKET_Raw *packet);
	void TDMA_ConfigureSecondSlot(TDMA_SecondSlot *slot);
	
#endif