#include "tdma_scheduler.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "efm32_timer.h"

#include "nRF24L01.h"

#include "radio.h"
#include "queue.h"

/* prototypes */
void TDMA_QueueTimingPacket();
void TDMA_SyncTimers(uint32_t time);

/* variables */
bool configSet = false,
	enabled = false,
	timers_syncd = false;
TDMA_Config config;

uint8_t timingPacket[32];

uint32_t timeQueueMem[TIME_QUEUE_SIZE];
queue_t timeQueue;

TIMER_Init_TypeDef timerInit =
{
	.enable     = true, 
	.debugRun   = true, 
	.prescale   = timerPrescale1024, 
	.clkSel     = timerClkSelHFPerClk, 
	.fallAction = timerInputActionNone, 
	.riseAction = timerInputActionNone, 
	.mode       = timerModeUp, 
	.dmaClrAct  = false,
	.quadModeX4 = false, 
	.oneShot    = false, 
	.sync       = false, 
};

TIMER_InitCC_TypeDef timerCCOff = 
{
	.cufoa      = timerOutputActionNone,
	.cofoa      = timerOutputActionNone,
	.cmoa       = timerOutputActionNone,
	.mode       = timerCCModeOff,
	.filter     = true,
	.prsInput   = false,
	.coist      = false,
	.outInvert  = false,
};

TIMER_InitCC_TypeDef timerCCCompare = 
{
	.cufoa      = timerOutputActionNone,
	.cofoa      = timerOutputActionNone,
	.cmoa       = timerOutputActionNone,
	.mode       = timerCCModeCompare,
	.filter     = true,
	.prsInput   = false,
	.coist      = false,
	.outInvert  = false,
};

TIMER_InitCC_TypeDef timerCCCe = 
{
	.cufoa      = timerOutputActionNone,
	.cofoa      = timerOutputActionNone,
	.cmoa       = timerOutputActionSet,
	.mode       = timerCCModeCompare,
	.filter     = true,
	.prsInput   = false,
	.coist      = false,
	.outInvert  = false,
};

TIMER_InitCC_TypeDef timerCCIrq = 
{
	.eventCtrl	= timerEventFalling,
	.edge		= timerEdgeFalling,
	.cufoa      = timerOutputActionNone,
	.cofoa      = timerOutputActionNone,
	.cmoa       = timerOutputActionNone,
	.mode       = timerCCModeCapture,
	.filter     = true,
	.prsInput   = false,
	.coist      = true,
	.outInvert  = false,
};

/* interrupts */
void TIMER0_IRQHandler()
{
	
	uint32_t flags = TIMER_IntGet(TIMER0);
	
	if (flags & TIMER_IF_CC0)
	{
		
		RADIO_SetMode(RADIO_OFF);
		
	}
	
	if (flags & TIMER_IF_CC1)
	{
		
		// disable system calls
		RADIO_EnableSystemCalls(false);
		
		// config tx
		RADIO_SetMode(RADIO_TX);
		
		// enable ce cc
		TIMER_InitCC(TIMER1, 0, &timerCCCe);
		
		// fill fifo
		RADIO_PacketUploadInit();
		RADIO_PacketUploadInit();
		
	}
	
	if (flags & TIMER_IF_CC2)
	{
		
		TDMA_SyncTimers(TIMER_CaptureGet(TIMER0, 2));
		
		TIMER_InitCC(TIMER0, 2, &timerCCOff);
		
	}
	
	TIMER_IntClear(TIMER0, flags);
	
}

void TIMER1_IRQHandler()
{
	
	uint32_t flags = TIMER_IntGet(TIMER1);
	
	if (flags & TIMER_IF_OF)
	{
		
		if (config.master)
		{
			
			// disable system calls
			RADIO_EnableSystemCalls(false);
			
			// config tx
			RADIO_SetMode(RADIO_TX);
			
			// queue timing packet
			TDMA_QueueTimingPacket();
			
			// enable ce cc
			TIMER_InitCC(TIMER1, 0, &timerCCCe);
			
			// fill fifo
			RADIO_PacketUploadInit();
			RADIO_PacketUploadInit();
			
		}
		else
		{
			
			// enable system calls
			RADIO_EnableSystemCalls(true);
			
			// enable capture
			TIMER_InitCC(TIMER0, 2, &timerCCIrq);
			
			// enable rx
			RADIO_SetMode(RADIO_RX);
			
		}
		
	}
	
	if (flags & TIMER_IF_CC0)
	{
		
		// enable system calls
		RADIO_EnableSystemCalls(true);
		
		// force fifo check
		RADIO_FifoCheckSetup();
		
		// disable ce cc
		TIMER_InitCC(TIMER1, 0, &timerCCOff);
		
	}
	
	if (flags & TIMER_IF_CC1)
	{
		
		RADIO_EnableSystemCalls(false);
		
	}
	
	if (flags & TIMER_IF_CC2)
	{
		
		if (config.master)
		{
			
			RADIO_SetMode(RADIO_RX);
			RADIO_EnableSystemCalls(true);
			
		}
		else
		{
			
			RADIO_SetMode(RADIO_OFF);
			
		}
		
	}
	
	TIMER_IntClear(TIMER1, flags);
	
}

/* functions */
void TDMA_Init(TDMA_Config *_config)
{
	
	// create time queue (used for initial clock sync)
	QUEUE_Init(&timeQueue, (uint8_t*)timeQueueMem, sizeof(uint32_t), TIME_QUEUE_SIZE);
	
	// create timing packet
	memset(timingPacket, 0, 32);
	
	// store settings
	memcpy((void*)&config, (void*)_config, sizeof(TDMA_Config));
	configSet = true;
	
	// config timers
	TIMER_TopSet(TIMER0, config.slotCount * (config.guardPeriod + config.transmitPeriod));
	TIMER_TopSet(TIMER1, config.slotCount * (config.guardPeriod + config.transmitPeriod));
	
	TIMER0->ROUTE |= (TIMER_ROUTE_CC2PEN | TIMER_ROUTE_LOCATION_LOC2);
	TIMER1->ROUTE |= (TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC4);
	
	// config CCs
	TIMER_InitCC(TIMER0, 2, &timerCCOff);
	TIMER_InitCC(TIMER1, 0, &timerCCOff);
	TIMER_InitCC(TIMER1, 1, &timerCCCompare);
	TIMER_InitCC(TIMER1, 2, &timerCCCompare);
	
	if (config.master)
	{
		TIMER_CompareSet(TIMER1, 0, config.guardPeriod);
		TIMER_CompareSet(TIMER1, 1, (config.guardPeriod + config.transmitPeriod) - config.protectionPeriod);
		TIMER_CompareSet(TIMER1, 2, config.guardPeriod + config.transmitPeriod);
	}
	else
	{
		TIMER_InitCC(TIMER0, 0, &timerCCCompare);
		TIMER_InitCC(TIMER0, 1, &timerCCCompare);
		TIMER_CompareSet(TIMER0, 0, config.guardPeriod + config.transmitPeriod);
		TIMER_CompareSet(TIMER0, 1, (config.guardPeriod + config.transmitPeriod) * config.slot + 1);
		TIMER_CompareSet(TIMER1, 0, config.guardPeriod + ((config.guardPeriod + config.transmitPeriod) * config.slot));
		TIMER_CompareSet(TIMER1, 1, ((config.guardPeriod + config.transmitPeriod) * (config.slot + 1))  - config.protectionPeriod);
		TIMER_CompareSet(TIMER1, 2, ((config.guardPeriod + config.transmitPeriod) * (config.slot + 1))  - 1);
	}
	
}

void TDMA_Enable(bool enable)
{
	
	if (!configSet)
		return;
	
	if (!enable)
	{
		timerInit.enable = false;
		enabled = false;
		
		TIMER_Init(TIMER0, &timerInit);
		TIMER_Init(TIMER1, &timerInit);
		
		return;
	}
	
	// config radio
	RADIO_SetMode(RADIO_OFF);
	RADIO_EnableSystemCalls(false);
	
	if (config.master)
	{
		
		RADIO_SetMode(RADIO_TX);
		
		// queue timing packet
		TDMA_QueueTimingPacket();
		
		// queue rest of packets
		RADIO_PacketUploadInit();
		RADIO_PacketUploadInit();
		
	}
	else
	{
		
		RADIO_SetMode(RADIO_RX);
		
	}
	
	// enable irqs
	if (config.master)
	{
		TIMER_IntEnable(TIMER1, TIMER_IF_OF);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC0);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC1);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC2);
	}
	else
	{
		TIMER_IntEnable(TIMER1, TIMER_IF_CC2);
	}
	
	// enable CCs
	if (config.master)
	{
		TIMER_InitCC(TIMER1, 0, &timerCCCe);
	}
	else
	{
		timers_syncd = false;
		TIMER_InitCC(TIMER0, 2, &timerCCIrq);
	}
	
	// reset & enable timers
	timerInit.enable = enable;
	
	TIMER_Reset(TIMER0);
	TIMER_Reset(TIMER1);
	
	enabled = true;
	
	TIMER_Init(TIMER0, &timerInit);
	TIMER_Init(TIMER1, &timerInit);
	
	// wait for timer sync
	if (!config.master)
	{
		
		uint8_t packet[32];
		uint32_t time;
		while (1)
		{
			if (RADIO_Recv(packet))
			{
				time = QUEUE_Dequeue(&timeQueue, (uint8_t*)&time);
				
				int i;
				bool isTimingPacket = true;
				for (i = 0; i < 32; i++)
					if (packet[i] != 0)
						isTimingPacket = false;
				
				if (isTimingPacket)
					break;
			}
			
			// enter emu 1 ?
			
		}
		
		timers_syncd = true;
		TDMA_SyncTimers(time);
		
		// enable all irqs
		TIMER_IntEnable(TIMER0, TIMER_IF_CC0);
		TIMER_IntEnable(TIMER0, TIMER_IF_CC1);
		TIMER_IntEnable(TIMER0, TIMER_IF_CC2);
		
		TIMER_IntEnable(TIMER1, TIMER_IF_OF);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC0);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC1);
		
	}
	
}

void TDMA_QueueTimingPacket()
{
	
	RADIO_DmaTransfer transfer;
	
	transfer.ctrl = NRF_W_TX_PAYLOAD;
	transfer.src = timingPacket;
	transfer.dst = NULL;
	transfer.len = 32;
	transfer.complete = NULL;
	transfer.systemCall = false;
	
	RADIO_QueueTransfer(&transfer);
	
}

void TDMA_SyncTimers(uint32_t time)
{
	
	if (!timers_syncd)
	{
		QUEUE_Queue(&timeQueue, (uint8_t*)&time);
		return;
	}
	
	time -= config.guardPeriod;
	
	if (time < 0)
		time += TIMER_TopGet(TIMER0); 
	
	int32_t diff = TIMER_CounterGet(TIMER0) - time;
	
	if (diff < 0)
		diff += TIMER_CounterGet(TIMER0);
	
	TIMER_CounterSet(TIMER0, diff);
	TIMER_CounterSet(TIMER1, diff);
	
}
