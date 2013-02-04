#ifndef __TDMA_H__
	
	#define __TDMA_H__
	
	#include <stdbool.h>
	#include <stdint.h>
	
	#define TDMA_INITIAL_SYNC_OFFSET 230
	
	typedef struct
	{
		bool master;
		uint32_t guardPeriod;
		uint32_t transmitPeriod;
		uint32_t protectionPeriod;
		uint8_t slot;
		uint8_t slotCount;
	}
	TDMA_Config;
	
	void TDMA_Init();
	void TDMA_Enable(bool enable);
	void TDMA_CheckSync();
	
	bool TDMA_IsTimingPacket(uint8_t packet[32]);
	void TDMA_TimingPacketReceived();
	void TDMA_RadioTransfer(uint8_t data[33]);
	
#endif