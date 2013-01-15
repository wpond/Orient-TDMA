#include "tdma_scheduler.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "efm32_gpio.h"
#include "efm32_int.h"
#include "efm32_timer.h"

#include "nRF24L01.h"

#include "config.h"
#include "radio.h"
#include "queue.h"
#include "led.h"

/* prototypes */
void TDMA_QueueTimingPacket();
void TDMA_SyncTimers(uint32_t time);
void TDMA_WaitForSync();

/* variables */
uint8_t TDMA_CONFIG_TX = 0x0E,
	TDMA_CONFIG_RX = 0x0F,
	TDMA_CONFIG_OFF = 0x0C;

bool configSet = false,
	enabled = false,
	timersSyncd = false;
volatile bool timingCaptured = false,
	captureNext = false;
TDMA_Config config;
uint16_t timingMisses = 0;

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
	.filter     = false,
	.prsInput   = false,
	.coist      = true,
	.outInvert  = false,
};

/* interrupts */
void TIMER0_IRQHandler()
{
	
	char tmsg[255];
	uint32_t flags = TIMER_IntGet(TIMER0);
	
	if (flags & TIMER_IF_CC2)
	{
		
		RADIO_IRQHandler();
		
		uint32_t time;
		
		time = TIMER_CaptureGet(TIMER0, 2);
		
		if (captureNext)
		{
		
			if (!timersSyncd)
			{
				QUEUE_Queue(&timeQueue,(uint8_t*)&time);
			}
			else
			{
				TDMA_SyncTimers(time);
				
				timingCaptured = true;
				captureNext = false;
			}
			
		}
		
		sprintf(tmsg,"%i: TIMER0 CC2\n", TIMER_CounterGet(TIMER1));
		TRACE(tmsg);
		
	}
	
	TIMER_IntClear(TIMER0, flags);
	
	if (flags & TIMER_IF_CC0)
	{
		
		LED_Off(RED);
		
		sprintf(tmsg,"\n%i: TIMER0 CC0\n", TIMER_CounterGet(TIMER1));
		TRACE(tmsg);
		
		// disable system calls
		RADIO_EnableSystemCalls(false);
		
		RADIO_SetMode(RADIO_OFF);
		
		if (timingCaptured)
		{
			timingMisses = 0;
		}
		else
		{
			timingMisses++;
		}
		timingCaptured = false;
		
		sprintf(tmsg,"Timing misses: %i\n", timingMisses);
		TRACE(tmsg);
		
		captureNext = false;
		
	}
	
	if (flags & TIMER_IF_CC1)
	{
		
		// config tx
		RADIO_SetMode(RADIO_TX);
		
		// enable ce cc
		TIMER_InitCC(TIMER1, 0, &timerCCCe);
		
		// fill fifo
		RADIO_PacketUploadInit();
		//RADIO_PacketUploadInit();
		//RADIO_PacketUploadInit();
		
		sprintf(tmsg,"%i: TIMER0 CC1\n", TIMER_CounterGet(TIMER1));
		TRACE(tmsg);
		
	}
	
	if (flags & TIMER_IF_ICBOF2)
	{
		RADIO_IRQHandler();
		sprintf(tmsg,"%i: CC2 BUFFER OVERFLOW \n",TIMER_CounterGet(TIMER0));
		TRACE(tmsg);
	}
	
}

void TIMER1_IRQHandler()
{
	
	char tmsg[255];
	
	uint32_t flags = TIMER_IntGet(TIMER1);
	TIMER_IntClear(TIMER1, flags);
	
	if (flags & TIMER_IF_OF)
	{
		
		sprintf(tmsg,"%i: TIMER1 OF\n", TIMER_CounterGet(TIMER1));
		TRACE(tmsg);
		
		if (config.master)
		{
						
			// disable system calls
			RADIO_EnableSystemCalls(false);
			
			// config tx
			RADIO_SetMode(RADIO_TX);
			
			RADIO_ClearIRQs();
			
			// queue timing packet
			TDMA_QueueTimingPacket();
			
			// enable ce cc
			TIMER_InitCC(TIMER1, 0, &timerCCCe);
			
			// fill fifo
			RADIO_PacketUploadInit();
			//RADIO_PacketUploadInit();
			
		}
		else
		{
			
			LED_On(RED);
			
			// enable rx
			RADIO_SetMode(RADIO_RX);
			
			captureNext = true;
			
			sprintf(tmsg,"%i: IRQ capture enabled\n", TIMER_CounterGet(TIMER1));
			TRACE(tmsg);
			
			// enable system calls
			RADIO_EnableSystemCalls(true);
			
		}
		
	}
	
	if (flags & TIMER_IF_CC0)
	{
		
		LED_On(GREEN);
		
		sprintf(tmsg,"%i: TIMER1 CC0\n", TIMER_CounterGet(TIMER1));
		TRACE(tmsg);
		
		// disable ce cc
		TIMER_InitCC(TIMER1, 0, &timerCCOff);
		
		// enable system calls
		RADIO_EnableSystemCalls(true);
		
	}
	
	if (flags & TIMER_IF_CC1)
	{
		
		sprintf(tmsg,"%i: TIMER1 CC1\n", TIMER_CounterGet(TIMER1));
		TRACE(tmsg);
		
		RADIO_EnableSystemCalls(false);
		
	}
	
	if (flags & TIMER_IF_CC2)
	{
		
		TIMER_IntClear(TIMER0,TIMER_IF_CC2);
		
		sprintf(tmsg,"%i: TIMER1 CC2\n", TIMER_CounterGet(TIMER1));
		TRACE(tmsg);
		
		if (config.master)
		{
			
			LED_Off(GREEN);
			LED_On(RED);
			RADIO_EnableSystemCalls(true);
			RADIO_SetMode(RADIO_RX);
			RADIO_FifoCheckSetup();
			
		}
		else
		{
			
			LED_Off(GREEN);
			RADIO_SetMode(RADIO_OFF);
			
		}
		
	}
	
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
	
	if (config.master)
	{
		TRACE("TDMA configured as master\n");
	}
	else
	{
		TRACE("TDMA configured as slave\n");
	}
	
}

void TDMA_Enable(bool enable)
{
	
	if (!configSet)
		return;
	
	TIMER_Reset(TIMER0);
	TIMER_Reset(TIMER1);
	
	TIMER_TopSet(TIMER0, (config.slotCount - 1) * (config.guardPeriod + config.transmitPeriod));
	TIMER_TopSet(TIMER1, (config.slotCount - 1) * (config.guardPeriod + config.transmitPeriod));
	
	char tmsg[256];
	sprintf(tmsg, "Top: %i\n", TIMER_TopGet(TIMER1));
	TRACE(tmsg);
	
	TIMER1->ROUTE |= (TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC4);
	
	if (!config.master)
	{
		TIMER0->ROUTE |= (TIMER_ROUTE_CC2PEN | TIMER_ROUTE_LOCATION_LOC2);
	}
	
	// disable irqs
	INT_Disable();
	TIMER_IntDisable(TIMER0, TIMER_IF_CC0);
	TIMER_IntDisable(TIMER0, TIMER_IF_CC1);
	TIMER_IntDisable(TIMER0, TIMER_IF_CC2);
	
	TIMER_IntDisable(TIMER1, TIMER_IF_OF);
	TIMER_IntDisable(TIMER1, TIMER_IF_CC0);
	TIMER_IntDisable(TIMER1, TIMER_IF_CC1);
	TIMER_IntDisable(TIMER1, TIMER_IF_CC2);
	INT_Enable();
	
	if (!enable)
	{
		timerInit.enable = false;
		enabled = false;
		
		TIMER_Init(TIMER0, &timerInit);
		TIMER_Init(TIMER1, &timerInit);
		
		TRACE("TDMA Disabled\n");
		
		//GPIO_PinModeSet(NRF_INT_PORT, NRF_INT_PIN, gpioModeInput, 0);
		NVIC_EnableIRQ(GPIO_EVEN_IRQn);
		
		return;
	}
	
	// config radio
	RADIO_SetMode(RADIO_OFF);
	RADIO_EnableSystemCalls(false);
	RADIO_WriteRegister(NRF_RF_CH, config.channel);
	
	if (config.master)
	{
		
		RADIO_SetMode(RADIO_TX);
		
		// queue timing packet
		TDMA_QueueTimingPacket();
		
		// queue rest of packets
		RADIO_PacketUploadInit();
		//RADIO_PacketUploadInit();
		
		TIMER_InitCC(TIMER1, 0, &timerCCCe);
		
		TIMER_CompareSet(TIMER1, 0, config.guardPeriod);
		TIMER_CompareSet(TIMER1, 1, (config.guardPeriod + config.transmitPeriod) - config.protectionPeriod);
		TIMER_CompareSet(TIMER1, 2, config.guardPeriod + config.transmitPeriod);
		
		TIMER_IntEnable(TIMER1, TIMER_IF_OF);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC0);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC1);
		TIMER_IntEnable(TIMER1, TIMER_IF_CC2);
		
		
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
		TRACE(tmsg);
		
	}
	else
	{
		
		NVIC_DisableIRQ(GPIO_EVEN_IRQn);
		//GPIO_PinModeSet(NRF_INT_PORT, NRF_INT_PIN, gpioModeDisabled, 0);
		
		TIMER_CompareSet(TIMER0, 0, config.guardPeriod + config.transmitPeriod);
		TIMER_CompareSet(TIMER0, 1, (config.guardPeriod + config.transmitPeriod) * config.slot + 1);
		TIMER_CompareSet(TIMER1, 0, config.guardPeriod + ((config.guardPeriod + config.transmitPeriod) * config.slot));
		TIMER_CompareSet(TIMER1, 1, ((config.guardPeriod + config.transmitPeriod) * (config.slot + 1))  - config.protectionPeriod);
		TIMER_CompareSet(TIMER1, 2, ((config.guardPeriod + config.transmitPeriod) * (config.slot + 1))  - 1);
		
		// reset & enable timers
		timerInit.enable = enable;
		
		enabled = true;
		
		TIMER_Init(TIMER0, &timerInit);
		TIMER_Init(TIMER1, &timerInit);
		
		TDMA_WaitForSync();
		
		char tmsg[256];
		sprintf(tmsg, "%i: TDMA Enabled\n", TIMER_CounterGet(TIMER1));
		TRACE(tmsg);
			
	}
	
}

void TDMA_WaitForSync()
{
	
	INT_Disable();
	TIMER_InitCC(TIMER0, 0, &timerCCOff);
	TIMER_InitCC(TIMER0, 1, &timerCCOff);
	TIMER_InitCC(TIMER0, 2, &timerCCOff);
	
	TIMER_InitCC(TIMER1, 0, &timerCCOff);
	TIMER_InitCC(TIMER1, 1, &timerCCOff);
	TIMER_InitCC(TIMER1, 2, &timerCCOff);
	
	TIMER_IntDisable(TIMER0, TIMER_IF_CC0);
	TIMER_IntDisable(TIMER0, TIMER_IF_CC1);
	TIMER_IntDisable(TIMER0, TIMER_IF_CC2);
	
	TIMER_IntDisable(TIMER1, TIMER_IF_OF);
	TIMER_IntDisable(TIMER1, TIMER_IF_CC0);
	TIMER_IntDisable(TIMER1, TIMER_IF_CC1);
	TIMER_IntDisable(TIMER1, TIMER_IF_CC2);
	INT_Enable();
	
	//timerCCIrq.coist = GPIO_PinInGet(NRF_INT_PORT,NRF_INT_PIN) == 1;
	TIMER_InitCC(TIMER0, 2, &timerCCIrq);
	
	RADIO_EnableSystemCalls(true);
	RADIO_SetMode(RADIO_RX);
	
	LED_On(RED);
	
	timersSyncd = false;
	TRACE("Empty time queue\n");
	QUEUE_Empty(&timeQueue);
	
	// wait for radio to switch mode before enabling irq
	RADIO_ReadRegister(NRF_CONFIG);
	
	TIMER_IntEnable(TIMER0, TIMER_IF_CC2);
	TIMER_IntEnable(TIMER0, TIMER_IF_ICBOF2);
	
	uint8_t packet[32];
	
	timersSyncd = false;
	captureNext = true;
	
	while (!timersSyncd)
	{
		if (RADIO_Recv(packet))
		{
			
			char tmsg[255];
			sprintf(tmsg,"Packet received\n");
			TRACE(tmsg);
			
		}
		
		// enter emu 1 ?
		
	}
	
	timingMisses = 0;
	LED_Off(RED);
	
	// enable all irqs
	INT_Disable();
	TIMER_InitCC(TIMER0, 0, &timerCCCompare);
	TIMER_InitCC(TIMER0, 1, &timerCCCompare);
	
	TIMER_InitCC(TIMER1, 0, &timerCCOff);
	TIMER_InitCC(TIMER1, 1, &timerCCCompare);
	TIMER_InitCC(TIMER1, 2, &timerCCCompare);
	
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
	
	TRACE("Sync'd\n");
	
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
	
	INT_Disable();
	time = TIMER_CounterGet(TIMER0) - (time - config.guardPeriod);
	
	while (time < 0)
		time += TIMER_TopGet(TIMER0); 
	
	TIMER_CounterSet(TIMER0, time);
	TIMER_CounterSet(TIMER1, time);
	
	timersSyncd = true;
	INT_Enable();
	
	char tmsg[255];
	sprintf(tmsg, "Time sync'd to: %i\n", time);
	TRACE(tmsg);
	
}

bool TDMA_IsTimingPacket(uint8_t packet[32])
{
	
	uint32_t time;
	
	if (!timersSyncd)
		if (!QUEUE_Dequeue(&timeQueue,(uint8_t*)&time))
			return false;
	
	if (memcmp(packet,timingPacket,32) != 0)
		return false;
	
	char tmsg[255];
	sprintf(tmsg,"%i: TIMING PACKET FOUND\n", TIMER_CounterGet(TIMER0));
	TRACE(tmsg);
	
	if (!timersSyncd)
	{
		captureNext = false;
		
		TDMA_SyncTimers(time);
	}
	
	return true;
	
}

bool TDMA_IsEnabled()
{
	
	return enabled;
	
}

void TDMA_CheckSync()
{
	
	if (timingMisses > 3)
	{
		RADIO_EnableSystemCalls(false);
		RADIO_SetMode(RADIO_OFF);
		RADIO_ReadRegister(NRF_STATUS);
		TDMA_Enable(true);
	}
	
}
