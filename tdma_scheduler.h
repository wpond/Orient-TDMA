#ifndef __TDMA_SCHEDULER_H__
	
	#define __TDMA_SCHEDULER_H__
	
	#include <stdint.h>
	#include <stdbool.h>
	
	typedef struct
	{
		bool master;
		uint8_t channel,
			slot,
			slotCount;
		uint32_t guardPeriod,
			transmitPeriod,
			protectionPeriod;
	}
	TDMA_Config;
	
	#define TIME_QUEUE_SIZE RADIO_RECV_QUEUE_SIZE
	
	void TDMA_Init(TDMA_Config *_config);
	void TDMA_Enable(bool enable);
	
#endif