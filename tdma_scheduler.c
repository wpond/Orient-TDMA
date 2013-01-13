#include "tdma_scheduler.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "efm32_int.h"
#include "efm32_timer.h"

#include "nRF24L01.h"

#include "config.h"
#include "radio.h"
#include "queue.h"

/* prototypes */
void TDMA_QueueTimingPacket();
void TDMA_SyncTimers(uint32_t time);

/* variables */
uint8_t TDMA_CONFIG_TX = 0x0E,
	TDMA_CONFIG_RX = 0x0F,
	TDMA_CONFIG_OFF = 0x0C;

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
	.debugRun   = false, 
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
	char tmsg[255];
	
	if (flags & TIMER_IF_CC0)
	{
		
		sprintf(tmsg,"%i: TIMER0 CC0\n", TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
		
		// disable system calls
		RADIO_EnableSystemCalls(false);
		
		NRF_CE_lo;
		NRF_RXEN_lo;
		
		RADIO_DmaTransfer transfer;
		
		transfer.ctrl = NRF_CONFIG;
		transfer.src = &TDMA_CONFIG_OFF;
		transfer.dst = NULL;
		transfer.len = 1;
		transfer.complete = NULL;
		transfer.systemCall = false;
		
		RADIO_QueueTransfer(&transfer);
		
		currentMode = RADIO_OFF;
		
	}
	
	if (flags & TIMER_IF_CC1)
	{
		
		// config tx
		NRF_CE_lo;
		NRF_RXEN_lo;
		
		RADIO_DmaTransfer transfer;
		
		transfer.ctrl = NRF_CONFIG;
		transfer.src = &TDMA_CONFIG_TX;
		transfer.dst = NULL;
		transfer.len = 1;
		transfer.complete = NULL;
		transfer.systemCall = false;
		
		RADIO_QueueTransfer(&transfer);
		
		transfer.ctrl = NRF_FLUSH_TX;
		transfer.src = NULL;
		transfer.len = 0;
		
		RADIO_QueueTransfer(&transfer);
		
		transfer.ctrl = NRF_FLUSH_RX;
		
		RADIO_QueueTransfer(&transfer);
		
		currentMode = RADIO_TX;
		
		// enable ce cc
		TIMER_InitCC(TIMER1, 0, &timerCCCe);
		
		// fill fifo
		RADIO_PacketUploadInit();
		//RADIO_PacketUploadInit();
		
		sprintf(tmsg,"%i: TIMER0 CC1\n", TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
		
	}
	
	if (flags & TIMER_IF_CC2)
	{
		
		TDMA_SyncTimers(TIMER_CaptureGet(TIMER0, 2));
		
		TIMER_InitCC(TIMER0, 2, &timerCCOff);
		
		sprintf(tmsg,"%i: TIMER0 CC2\n", TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
		
	}
	
	TIMER_IntClear(TIMER0, flags);
	
}

void TIMER1_IRQHandler()
{
	
	char tmsg[255];
	
	uint32_t flags = TIMER_IntGet(TIMER1);
	
	if (flags & TIMER_IF_OF)
	{
		
		sprintf(tmsg,"%i: TIMER1 OF\n", TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
		
		if (config.master)
		{
			
			// disable system calls
			RADIO_EnableSystemCalls(false);
			
			// config tx
			NRF_CE_lo;
			NRF_RXEN_lo;
			
			RADIO_DmaTransfer transfer;
			
			transfer.ctrl = NRF_CONFIG;
			transfer.src = &TDMA_CONFIG_TX;
			transfer.dst = NULL;
			transfer.len = 1;
			transfer.complete = NULL;
			transfer.systemCall = false;
			
			RADIO_QueueTransfer(&transfer);
			
			transfer.ctrl = NRF_FLUSH_TX;
			transfer.src = NULL;
			transfer.len = 0;
			
			RADIO_QueueTransfer(&transfer);
			
			transfer.ctrl = NRF_FLUSH_RX;
			
			RADIO_QueueTransfer(&transfer);
			
			currentMode = RADIO_TX;
			
			// queue timing packet
			TDMA_QueueTimingPacket();
			
			// enable ce cc
			TIMER_InitCC(TIMER1, 0, &timerCCCe);
			
			// fill fifo
			//RADIO_PacketUploadInit();
			//RADIO_PacketUploadInit();
			
		}
		else
		{
			
			// enable rx
			NRF_CE_lo;
			NRF_RXEN_lo;
			
			RADIO_DmaTransfer transfer;
			
			transfer.ctrl = NRF_CONFIG;
			transfer.src = &TDMA_CONFIG_RX;
			transfer.dst = NULL;
			transfer.len = 1;
			transfer.complete = NULL;
			transfer.systemCall = false;
			
			RADIO_QueueTransfer(&transfer);
			
			transfer.ctrl = NRF_FLUSH_TX;
			transfer.src = NULL;
			transfer.len = 0;
			
			RADIO_QueueTransfer(&transfer);
			
			transfer.ctrl = NRF_FLUSH_RX;
			
			RADIO_QueueTransfer(&transfer);
			
			currentMode = RADIO_RX;
			
			NRF_CE_hi;
			NRF_RXEN_hi;
			
			// enable system calls
			RADIO_EnableSystemCalls(true);
			
			// enable capture
			TIMER_InitCC(TIMER0, 2, &timerCCIrq);
			
		}
		
	}
	
	if (flags & TIMER_IF_CC0)
	{
		
		sprintf(tmsg,"%i: TIMER1 CC0\n", TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
		
		// enable system calls
		RADIO_EnableSystemCalls(true);
		
		// force fifo check
		RADIO_FifoCheckSetup();
		
		// disable ce cc
		TIMER_InitCC(TIMER1, 0, &timerCCOff);
		
	}
	
	if (flags & TIMER_IF_CC1)
	{
		
		sprintf(tmsg,"%i: TIMER1 CC1\n", TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
		
		RADIO_EnableSystemCalls(false);
		
	}
	
	if (flags & TIMER_IF_CC2)
	{
		
		sprintf(tmsg,"%i: TIMER1 CC2\n", TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
		
		if (config.master)
		{
			
			NRF_CE_lo;
			NRF_RXEN_lo;
			
			RADIO_DmaTransfer transfer;
			
			transfer.ctrl = NRF_CONFIG;
			transfer.src = &TDMA_CONFIG_RX;
			transfer.dst = NULL;
			transfer.len = 1;
			transfer.complete = NULL;
			transfer.systemCall = false;
			
			RADIO_QueueTransfer(&transfer);
			
			transfer.ctrl = NRF_FLUSH_TX;
			transfer.src = NULL;
			transfer.len = 0;
			
			RADIO_QueueTransfer(&transfer);
			
			transfer.ctrl = NRF_FLUSH_RX;
			
			RADIO_QueueTransfer(&transfer);
			
			currentMode = RADIO_RX;
			
			NRF_CE_hi;
			NRF_RXEN_hi;
			
			RADIO_EnableSystemCalls(true);
			
		}
		else
		{
			
			NRF_CE_lo;
			NRF_RXEN_lo;
			
			RADIO_DmaTransfer transfer;
			
			transfer.ctrl = NRF_CONFIG;
			transfer.src = &TDMA_CONFIG_OFF;
			transfer.dst = NULL;
			transfer.len = 1;
			transfer.complete = NULL;
			transfer.systemCall = false;
			
			RADIO_QueueTransfer(&transfer);
			
			currentMode = RADIO_OFF;
			
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
	TIMER_Reset(TIMER0);
	TIMER_Reset(TIMER1);
	
	// disable irqs
	TIMER_IntDisable(TIMER0, TIMER_IF_CC0);
	TIMER_IntDisable(TIMER0, TIMER_IF_CC1);
	TIMER_IntDisable(TIMER0, TIMER_IF_CC2);
	
	TIMER_IntDisable(TIMER1, TIMER_IF_OF);
	TIMER_IntDisable(TIMER1, TIMER_IF_CC0);
	TIMER_IntDisable(TIMER1, TIMER_IF_CC1);
	TIMER_IntDisable(TIMER1, TIMER_IF_CC2);
	
	TIMER_TopSet(TIMER0, (config.slotCount - 1) * (config.guardPeriod + config.transmitPeriod));
	TIMER_TopSet(TIMER1, (config.slotCount - 1) * (config.guardPeriod + config.transmitPeriod));
	
	char tmsg[256];
	sprintf(tmsg, "Top: %i\n", TIMER_TopGet(TIMER1));
	//TRACE(tmsg);
	
	TIMER0->ROUTE |= (TIMER_ROUTE_CC2PEN | TIMER_ROUTE_LOCATION_LOC2);
	TIMER1->ROUTE |= (TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC4);
	
	if (config.master)
	{
		//TRACE("TDMA configured as master\n");
	}
	else
	{
		//TRACE("TDMA configured as slave\n");
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
		
		//TRACE("TDMA Disabled\n");
		
		return;
	}
	
	// config radio
	RADIO_SetMode(RADIO_OFF);
	RADIO_WriteRegister(NRF_RF_CH, config.channel);
	RADIO_EnableSystemCalls(false);
	
	if (config.master)
	{
		
		RADIO_SetMode(RADIO_TX);
		
		// queue timing packet
		TDMA_QueueTimingPacket();
		
		// queue rest of packets
		//RADIO_PacketUploadInit();
		//RADIO_PacketUploadInit();
		
		TIMER_InitCC(TIMER1, 0, &timerCCCe);
		
		TIMER_CompareSet(TIMER1, 0, config.guardPeriod);
		TIMER_CompareSet(TIMER1, 1, (config.guardPeriod + config.transmitPeriod) - config.protectionPeriod);
		TIMER_CompareSet(TIMER1, 2, config.guardPeriod + config.transmitPeriod);
		
		TIMER_IntEnable(TIMER1, TIMER_IF_OF);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC0);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC1);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC2);
		
		TIMER_InitCC(TIMER0, 2, &timerCCOff);
		TIMER_InitCC(TIMER1, 0, &timerCCOff);
		TIMER_InitCC(TIMER1, 1, &timerCCCompare);
		TIMER_InitCC(TIMER1, 2, &timerCCCompare);
		
		// reset & enable timers
		timerInit.enable = enable;
		
		enabled = true;
		
		TIMER_Init(TIMER0, &timerInit);
		TIMER_Init(TIMER1, &timerInit);
		
		char tmsg[256];
		sprintf(tmsg, "%i: TDMA Enabled\n", TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
		
	}
	else
	{
		
		timers_syncd = false;
		TIMER_InitCC(TIMER0, 2, &timerCCIrq);
		
		TIMER_CompareSet(TIMER0, 0, config.guardPeriod + config.transmitPeriod);
		TIMER_CompareSet(TIMER0, 1, (config.guardPeriod + config.transmitPeriod) * config.slot + 1);
		TIMER_CompareSet(TIMER1, 0, config.guardPeriod + ((config.guardPeriod + config.transmitPeriod) * config.slot));
		TIMER_CompareSet(TIMER1, 1, ((config.guardPeriod + config.transmitPeriod) * (config.slot + 1))  - config.protectionPeriod);
		TIMER_CompareSet(TIMER1, 2, ((config.guardPeriod + config.transmitPeriod) * (config.slot + 1))  - 1);
		
		TIMER_InitCC(TIMER0, 0, &timerCCOff);
		TIMER_InitCC(TIMER0, 1, &timerCCOff);
		
		TIMER_InitCC(TIMER1, 0, &timerCCOff);
		TIMER_InitCC(TIMER1, 1, &timerCCOff);
		TIMER_InitCC(TIMER1, 2, &timerCCOff);
		
		// reset & enable timers
		timerInit.enable = enable;
		
		enabled = true;
			
		// wait for timer sync
		
		RADIO_EnableSystemCalls(true);
		RADIO_SetMode(RADIO_RX);
		
		TIMER_IntClear(TIMER0, TIMER_IF_CC0);
		TIMER_IntClear(TIMER0, TIMER_IF_CC1);
		TIMER_IntClear(TIMER0, TIMER_IF_CC2);
		
		TIMER_Init(TIMER0, &timerInit);
		TIMER_Init(TIMER1, &timerInit);
		
		TIMER_IntEnable(TIMER0, TIMER_IF_CC2);
		
		uint8_t packet[32];
		uint32_t time;
		
		while (1)
		{
			if (RADIO_Recv(packet))
			{
				QUEUE_Dequeue(&timeQueue, (uint8_t*)&time);
				
				char tmsg[255];
				sprintf(tmsg,"%u: Packet received\n", (unsigned int)time);
				//TRACE(tmsg);
				
				if (memcmp(packet,timingPacket,32) == 0)
					break;
				
			}
			
			// enter emu 1 ?
			
		}
		
		timers_syncd = true;
		TDMA_SyncTimers(time);
		
		// enable all irqs
		TIMER_InitCC(TIMER0, 0, &timerCCCompare);
		TIMER_InitCC(TIMER0, 1, &timerCCCompare);
		
		TIMER_InitCC(TIMER1, 0, &timerCCOff);
		TIMER_InitCC(TIMER1, 1, &timerCCCompare);
		TIMER_InitCC(TIMER1, 2, &timerCCCompare);
		
		INT_Disable();
		TIMER_IntClear(TIMER0, TIMER_IF_CC0);
		TIMER_IntClear(TIMER0, TIMER_IF_CC1);
		
		TIMER_IntClear(TIMER1, TIMER_IF_OF);
		TIMER_IntClear(TIMER1, TIMER_IF_CC0);
		TIMER_IntClear(TIMER1, TIMER_IF_CC1);
		TIMER_IntClear(TIMER1, TIMER_IF_CC2);
		
		TIMER_IntEnable(TIMER0, TIMER_IF_CC0);
		TIMER_IntEnable(TIMER0, TIMER_IF_CC1);
		
		TIMER_IntEnable(TIMER1, TIMER_IF_OF);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC0);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC1);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC2);
		INT_Enable();
		
		//TRACE("Sync'd\n");
		
		char tmsg[256];
		sprintf(tmsg, "%i: TDMA Enabled\n", TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
			
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
	
	time = TIMER_CounterGet(TIMER0) - (time - config.guardPeriod);
	
	if (time < 0)
		time += TIMER_TopGet(TIMER0); 
	
	TIMER_CounterSet(TIMER0, time);
	TIMER_CounterSet(TIMER1, time);
	
	char tmsg[255];
	sprintf(tmsg, "Time sync'd to: %i\n", time);
	//TRACE(tmsg);
	
}

bool TDMA_IsTimingPacket(uint8_t packet[32])
{
	
	return timers_syncd && memcmp(packet,timingPacket,32) == 0;
	
}

bool TDMA_IsEnabled()
{
	
	return enabled;
	
}
