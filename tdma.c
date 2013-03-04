#include "tdma.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "efm32_dma.h"
#include "efm32_int.h"
#include "efm32_gpio.h"
#include "efm32_timer.h"

#include "nRF24L01.h"

#include "config.h"
#include "dma.h"
#include "aloha.h"
#include "radio.h"
#include "transport.h"
#include "alloc.h"

/* prototypes */
void TDMA_QueuePacket();
void TDMA_QueueTimingPacket();
void TDMA_EnableTxCC(bool enable);
void TDMA_SyncTimers(uint32_t time);
void TDMA_RadioTransferComplete(unsigned int channel, bool primary, void *transfer);
void TDMA_SendAck(PACKET_Raw *packet);
void TDMA_SendSlotEvent();
void TDMA_EnableSecondSlot(bool enable);
void TDMA_SetSecondSlotReloadCallback(uint32_t time);

/* variables */
TDMA_Config config;
TDMA_SecondSlot secondSlot;
bool syncTimers = false;

bool timingPacketReceived = false;

uint8_t timingPacket[33],
	txPacket[33],
	tdmaScratch;

DMA_CB_TypeDef cb;

bool timersSyncd = false;
volatile uint16_t syncMissCount = 0;
uint8_t txCount = 0;

bool secondSlotReloadEnabled = true;
uint32_t secondSlotReloadCallback;

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
	
	//INT_Disable();
	uint32_t flags = TIMER_IntGet(TIMER0);
	char tmsg[255];
	
	if (flags & TIMER_IF_CC2)
	{
		
		uint32_t time = TIMER_CaptureGet(TIMER0,2);
		
		
		if (!config.master && syncTimers)
		{
			syncTimers = false;
			TDMA_SyncTimers(time);
		}
		
		sprintf(tmsg,"%i: TIMER0 CC2 (RADIO IRQ)\n",(int)TIMER_CounterGet(TIMER0));
		//TRACE(tmsg);
		RADIO_IRQHandler();
		txCount++;
	}
	
	TIMER_IntClear(TIMER0,flags);
	//INT_Enable();
	
	if (flags & TIMER_IF_CC0)
	{
		RADIO_SetMode(RADIO_OFF);
		
		sprintf(tmsg,"%i: TIMER0 CC0\n",(int)TIMER_CounterGet(TIMER0));
		//TRACE(tmsg);
	}
	
	if (flags & TIMER_IF_CC1)
	{
		if (!syncTimers)
		{
			syncMissCount = 0;
		}
		else
		{
			syncMissCount++;
		}
		
		syncTimers = false;
		
		RADIO_EnableAutoTransmit(false);
		RADIO_SetMode(RADIO_TX);
		TDMA_EnableTxCC(true);
		TDMA_QueuePacket(); // dup push with transport_reload
		TRANSPORT_Reload();
		
		sprintf(tmsg,"%i: TIMER0 CC1\n",(int)TIMER_CounterGet(TIMER0));
		//TRACE(tmsg);
	}
	
}

void TIMER1_IRQHandler()
{
	
	uint32_t flags = TIMER_IntGet(TIMER1);
	char tmsg[255];
	
	if (flags & TIMER_IF_OF)
	{
		if (config.master)
		{
			RADIO_EnableAutoTransmit(false);
			RADIO_SetMode(RADIO_TX);
			TDMA_EnableTxCC(true);
			TRANSPORT_Reset();
			//TDMA_SendSlotEvent();
			TDMA_QueueTimingPacket();
			//TRANSPORT_ReloadReady();
		}
		else
		{
			TRANSPORT_Reset();
			RADIO_SetMode(RADIO_RX);
			syncTimers = true;
		}
		
		sprintf(tmsg,"%i: IRQ count = %i\n",(int)TIMER_CounterGet(TIMER1),txCount);
		TRACE(tmsg);
		
		txCount = 0;
	}
	
	if (flags & TIMER_IF_CC0)
	{
		if (config.master)
		{
			
			TDMA_EnableTxCC(false);
			RADIO_EnableAutoTransmit(true);
			
		}
		else
		{
			
			TDMA_EnableTxCC(false);
			RADIO_EnableAutoTransmit(true);
			
		}
		
		sprintf(tmsg,"%i: TIMER1 CC0\n",(int)TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
	}
	
	if (flags & TIMER_IF_CC1)
	{
		if (config.master)
		{
			
			RADIO_EnableAutoTransmit(false);
			
		}
		else
		{
			
			RADIO_EnableAutoTransmit(false);
			
		}
		
		sprintf(tmsg,"%i: TIMER1 CC1\n",(int)TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
	}
	
	if (flags & TIMER_IF_CC2)
	{
		if (config.master)
		{
			
			RADIO_SetMode(RADIO_RX);
			
		}
		else
		{
			
			RADIO_SetMode(RADIO_OFF);
			
		}
		
		sprintf(tmsg,"%i: TIMER1 CC2\n",(int)TIMER_CounterGet(TIMER1));
		//TRACE(tmsg);
	}
	
	TIMER_IntClear(TIMER1,flags);
	
}

void TIMER3_IRQHandler()
{
	
	uint32_t flags = TIMER_IntGet(TIMER3);
	char tmsg[255];
	
	if (flags & TIMER_IF_CC0)
	{
		TIMER_CompareSet(TIMER1, 0, config.guardPeriod + (config.guardPeriod + config.transmitPeriod) * secondSlot.slot);
		RADIO_EnableAutoTransmit(false);
		RADIO_SetMode(RADIO_TX);
		TDMA_EnableTxCC(true);
		TRANSPORT_ReloadReady();
		TRANSPORT_Reload();
		//TDMA_QueuePacket();
		
		if (secondSlot.len > 1)
		{
			secondSlotReloadEnabled = true;
			secondSlotReloadCallback = (config.guardPeriod + config.transmitPeriod) * (secondSlot.slot) + config.guardPeriod + config.transmitPeriod/2;
			TDMA_SetSecondSlotReloadCallback(secondSlotReloadCallback);
		}
		else
		{
			secondSlotReloadEnabled = false;
			TDMA_SetSecondSlotReloadCallback((config.guardPeriod + config.transmitPeriod) * (secondSlot.slot + secondSlot.len) - 1);
		}
		
		sprintf(tmsg,"%i: TIM3_CC0\n",(int)TIMER_CounterGet(TIMER3));
		TRACE(tmsg);
	}
	if (flags & TIMER_IF_CC1)
	{
		TIMER_CompareSet(TIMER1, 0, config.guardPeriod + ((config.guardPeriod + config.transmitPeriod) * config.slot));
		RADIO_EnableAutoTransmit(false);
		secondSlotReloadEnabled = false;
		TDMA_SetSecondSlotReloadCallback((config.guardPeriod + config.transmitPeriod) * (secondSlot.slot + secondSlot.len) - 1);
		sprintf(tmsg,"%i: TIM3_CC1\n",(int)TIMER_CounterGet(TIMER3));
		TRACE(tmsg);
	}
	if (flags & TIMER_IF_CC2)
	{
		if (secondSlotReloadEnabled)
		{
			if (secondSlotReloadCallback >= (config.guardPeriod + config.transmitPeriod) * (secondSlot.slot + secondSlot.len))
			{
				TDMA_SetSecondSlotReloadCallback((config.guardPeriod + config.transmitPeriod) * (secondSlot.slot + secondSlot.len) - 1);
				secondSlotReloadEnabled = false;
			}
			else
			{
				secondSlotReloadCallback += config.transmitPeriod / 2;
				
				if (secondSlotReloadCallback > (config.guardPeriod + config.transmitPeriod) * (secondSlot.slot + secondSlot.len - 1))
					secondSlotReloadCallback = (config.guardPeriod + config.transmitPeriod) * (secondSlot.slot + secondSlot.len - 1) - 1;
				
				TDMA_SetSecondSlotReloadCallback(secondSlotReloadCallback);
				
				TRANSPORT_ReloadReady();
				TRANSPORT_Reload();
			}
		}
		else
		{
			if (secondSlot.lease > 0)
				secondSlot.lease--;
			if (secondSlot.lease == 0)
				TDMA_EnableSecondSlot(false);
			RADIO_SetMode(RADIO_OFF);
		}
		sprintf(tmsg,"%i: TIM3_CC2\n",(int)TIMER_CounterGet(TIMER3));
		TRACE(tmsg);
	}
	
	TIMER_IntClear(TIMER3,flags);
	
}

/* functions */
void TDMA_Init(TDMA_Config *_config)
{
	
	memcpy((void*)&config,(void*)_config,sizeof(TDMA_Config));
	
	memset((void*)timingPacket,0,33);
	timingPacket[0] = NRF_W_TX_PAYLOAD;
	timingPacket[1] = 0xFF;
	timingPacket[2] = 0x00;
	
}

void TDMA_Enable(bool enable)
{
	
	TIMER_Reset(TIMER0);
	TIMER_Reset(TIMER1);
	TIMER_Reset(TIMER3);
	
	if (!enable)
	{
		RADIO_SetChannel(NODE_CHANNEL);
		NVIC_EnableIRQ(GPIO_EVEN_IRQn);
		//TRACE("TDMA DISABLE\n");
		return;
	}
	
	NVIC_DisableIRQ(GPIO_EVEN_IRQn);
	
	RADIO_SetMode(RADIO_OFF);
	RADIO_SetChannel(config.channel);
	RADIO_EnableAutoTransmit(false);
	
	uint32_t top = config.slotCount * (config.guardPeriod + config.transmitPeriod);
	
	TIMER_TopSet(TIMER0,top);
	TIMER_TopSet(TIMER1,top);
	TIMER_TopSet(TIMER3,top);
	
	TIMER0->ROUTE |= (TIMER_ROUTE_CC2PEN | TIMER_ROUTE_LOCATION_LOC2);
	TIMER1->ROUTE |= (TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC4);
	
	TIMER_Init(TIMER0, &timerInit);
	TIMER_Init(TIMER1, &timerInit);
	TIMER_Init(TIMER3, &timerInit);
	
	if (config.master)
	{
		
		ALLOC_Init(5,5); // TODO: get these values from config
		
		TIMER_CompareSet(TIMER1, 0, config.guardPeriod);
		TIMER_CompareSet(TIMER1, 1, (config.guardPeriod + config.transmitPeriod) - config.protectionPeriod);
		TIMER_CompareSet(TIMER1, 2, config.guardPeriod + config.transmitPeriod);
		
		TIMER_InitCC(TIMER0, 2, &timerCCIrq);
		
		TIMER_InitCC(TIMER1, 0, &timerCCOff);
		TIMER_InitCC(TIMER1, 1, &timerCCCompare);
		TIMER_InitCC(TIMER1, 2, &timerCCCompare);
		
		TIMER_IntEnable(TIMER0,TIMER_IF_CC2);
		
		TIMER_IntEnable(TIMER1,TIMER_IF_OF);
		TIMER_IntEnable(TIMER1,TIMER_IF_CC0);
		TIMER_IntEnable(TIMER1,TIMER_IF_CC1);
		TIMER_IntEnable(TIMER1,TIMER_IF_CC2);
		
		return;
		
	}
	
	uint8_t packet[32];
	
	INT_Disable();
	TIMER_IntDisable(TIMER0,TIMER_IF_CC0);
	TIMER_IntDisable(TIMER0,TIMER_IF_CC1);
	
	TIMER_IntDisable(TIMER1,TIMER_IF_OF);
	TIMER_IntDisable(TIMER1,TIMER_IF_CC0);
	TIMER_IntDisable(TIMER1,TIMER_IF_CC1);
	TIMER_IntDisable(TIMER1,TIMER_IF_CC2);
	
	TIMER_IntDisable(TIMER3,TIMER_IF_CC0);
	TIMER_IntDisable(TIMER3,TIMER_IF_CC1);
	TIMER_IntDisable(TIMER3,TIMER_IF_CC2);
	
	TIMER_InitCC(TIMER0, 2, &timerCCIrq);
	TIMER_IntEnable(TIMER0,TIMER_IF_CC2);
	INT_Enable();
	
	timingPacketReceived = false;
	timersSyncd = false;
	RADIO_SetMode(RADIO_RX);
	
	while (true)
	{
		RADIO_Recv(packet);
		if (timingPacketReceived)
		{
			uint32_t time = TIMER_CounterGet(TIMER0) - TDMA_INITIAL_SYNC_OFFSET;
			if (time < 0)
				time += TIMER_TopGet(TIMER0);
			TDMA_SyncTimers(time);
			break;
		}
	}
	
	syncMissCount = 0;
	timersSyncd = true;
	
	TIMER_CompareSet(TIMER0, 0, config.guardPeriod + config.transmitPeriod);
	TIMER_CompareSet(TIMER0, 1, (config.guardPeriod + config.transmitPeriod) * config.slot + 1);
	TIMER_CompareSet(TIMER1, 0, config.guardPeriod + ((config.guardPeriod + config.transmitPeriod) * config.slot));
	TIMER_CompareSet(TIMER1, 1, ((config.guardPeriod + config.transmitPeriod) * (config.slot + 1))  - config.protectionPeriod);
	TIMER_CompareSet(TIMER1, 2, ((config.guardPeriod + config.transmitPeriod) * (config.slot + 1))  - 1);
	
	INT_Disable();
	TIMER_InitCC(TIMER0, 0, &timerCCCompare);
	TIMER_InitCC(TIMER0, 1, &timerCCCompare);
	
	TIMER_InitCC(TIMER1, 0, &timerCCOff);
	TIMER_InitCC(TIMER1, 1, &timerCCCompare);
	TIMER_InitCC(TIMER1, 2, &timerCCCompare);
	
	TIMER_IntClear(TIMER0,TIMER_IF_CC0);
	TIMER_IntClear(TIMER0,TIMER_IF_CC1);
	
	TIMER_IntClear(TIMER1,TIMER_IF_OF);
	TIMER_IntClear(TIMER1,TIMER_IF_CC0);
	TIMER_IntClear(TIMER1,TIMER_IF_CC1);
	TIMER_IntClear(TIMER1,TIMER_IF_CC2);
	
	if (config.slot > 1)
		TIMER_IntEnable(TIMER0,TIMER_IF_CC0);
	TIMER_IntEnable(TIMER0,TIMER_IF_CC1);
	
	TIMER_IntEnable(TIMER1,TIMER_IF_OF);
	TIMER_IntEnable(TIMER1,TIMER_IF_CC0);
	TIMER_IntEnable(TIMER1,TIMER_IF_CC1);
	TIMER_IntEnable(TIMER1,TIMER_IF_CC2);
	INT_Enable();
	
	secondSlot.enabled = false;
	
}
void TDMA_SyncTimers(uint32_t time)
{
	
	INT_Disable();
	time = TIMER_CounterGet(TIMER0) - (time - (config.guardPeriod + 14));

	while (time < 0)
		time += TIMER_TopGet(TIMER0); 

	TIMER_CounterSet(TIMER0, time);
	TIMER_CounterSet(TIMER1, time);
	TIMER_CounterSet(TIMER3, time);
	
	INT_Enable();

	char tmsg[255];
	sprintf(tmsg, "Time sync'd to: %i\n", (int)time);
	//TRACE(tmsg);
	
}

void TDMA_QueuePacket()
{
	
	if (RADIO_SendDequeue(&txPacket[1]))
	{
		txPacket[0] = NRF_W_TX_PAYLOAD;
		TDMA_RadioTransfer(txPacket);
	}
	
}

void TDMA_QueueTimingPacket()
{
	
	TDMA_RadioTransfer(timingPacket);
	
}

void TDMA_RadioTransfer(uint8_t data[33])
{
	
	while (true)
	{
		INT_Disable();
		if (!transferActive)
		{
			transferActive = true;
			break;
		}
		INT_Enable();
	}
	INT_Enable();
	
	if (data[2] == PACKET_TRANSPORT_DATA)
	{
		char tmsg[255];
		sprintf(tmsg,"%i: sending DATA packet %i/%i\n",(int)TIMER_CounterGet(TIMER1),(int)data[5],(int)data[4]);
		TRACE(tmsg);
	}
	
	DMA_CfgChannel_TypeDef  rxChnlCfg;
	DMA_CfgDescr_TypeDef    rxDescrCfg;
	DMA_CfgChannel_TypeDef  txChnlCfg;
	DMA_CfgDescr_TypeDef    txDescrCfg;
	
	cb.cbFunc  = TDMA_RadioTransferComplete;
	cb.userPtr = NULL;
	
	rxChnlCfg.highPri   = false;
	rxChnlCfg.enableInt = true;
	rxChnlCfg.select    = DMAREQ_USART0_RXDATAV;
	rxChnlCfg.cb        = &cb;
	DMA_CfgChannel(DMA_CHANNEL_TDMA_RX, &rxChnlCfg);
	
	rxDescrCfg.dstInc  = dmaDataIncNone;
	rxDescrCfg.srcInc  = dmaDataIncNone;
	rxDescrCfg.size    = dmaDataSize1;
	rxDescrCfg.arbRate = dmaArbitrate1;
	rxDescrCfg.hprot   = 0;
	DMA_CfgDescr(DMA_CHANNEL_TDMA_RX, true, &rxDescrCfg);
	
	txChnlCfg.highPri   = false;
	txChnlCfg.enableInt = false;
	txChnlCfg.select    = DMAREQ_USART0_TXBL;
	txChnlCfg.cb        = &cb;
	DMA_CfgChannel(DMA_CHANNEL_TDMA_TX, &txChnlCfg);
	
	txDescrCfg.dstInc  = dmaDataIncNone;
	txDescrCfg.srcInc  = dmaDataInc1;
	txDescrCfg.size    = dmaDataSize1;
	txDescrCfg.arbRate = dmaArbitrate1;
	txDescrCfg.hprot   = 0;
	DMA_CfgDescr(DMA_CHANNEL_TDMA_TX, true, &txDescrCfg);
	
	NRF_CSN_lo;
	
	DMA_ActivateBasic(DMA_CHANNEL_TDMA_RX,
                      true,
                      false,
                      (void*)&tdmaScratch,
                      (void *)&(USART0->RXDATA),
                      32); 
	DMA_ActivateBasic(DMA_CHANNEL_TDMA_TX,
                    true,
                    false,
                    (void *)&(USART0->TXDATA),
                    (void*)data,
                    32);
	
}

void TDMA_RadioTransferComplete(unsigned int channel, bool primary, void *transfer)
{
	
	switch (channel)
	{
	case DMA_CHANNEL_TDMA_TX:
		break;
	case DMA_CHANNEL_TDMA_RX:
		NRF_CSN_hi;
		transferActive = false;
		break;
	}
	
}

bool TDMA_IsTimingPacket(uint8_t packet[32])
{
	
	return memcmp((void*)packet,(void*)&timingPacket[1],32) == 0;
	
}

void TDMA_EnableTxCC(bool enable)
{
	
	if(enable)
	{
		TIMER_InitCC(TIMER1, 0, &timerCCCe);
	}
	else
	{
		TIMER_InitCC(TIMER1, 0, &timerCCOff);
	}
	
}

void TDMA_CheckSync()
{
	
	if (syncMissCount > 3)
	{
		TRACE("OUT OF SYNC\n");
		TDMA_EnableSecondSlot(false);
		TDMA_Enable(true);
	}
	
}

void TDMA_TimingPacketReceived()
{
	timingPacketReceived = true;
}

void TDMA_SendAck(PACKET_Raw *packet)
{
	#ifndef BASESTATION
		PACKET_TDMA *packetTDMA = (PACKET_TDMA*)packet->payload;
		
		PACKET_Raw ack;
		ack.addr = BASESTATION_ID;
		ack.type = PACKET_TDMA_ACK;
		PACKET_TDMA *ackTDMA = (PACKET_TDMA*)ack.payload;
		ackTDMA->seqNum = packetTDMA->seqNum;
		
		RADIO_Send((uint8_t*)&ack);
	#endif
}

bool TDMA_PacketConfigure(PACKET_Raw *packet)
{
	
	TDMA_SendAck(packet);
	
	PACKET_TDMA *packetTDMA = (PACKET_TDMA*)packet->payload;
	
	TDMA_Config *c = (TDMA_Config*)packetTDMA->payload;
	
	TDMA_Init(c);
	
	//TRACE("RECVD TDMA CONFIGURATION:\n");
	//TRACESTRUCT((void*)c,sizeof(TDMA_Config));
	
	if (packetTDMA->payload[sizeof(TDMA_Config)])
	{
		ALOHA_Enable(false);
		return true;
	}
	else
	{
		TDMA_Enable(false);
		ALOHA_Enable(true);
		return false;
	}
	
}

bool TDMA_PacketEnable(PACKET_Raw *packet)
{
	
	TDMA_SendAck(packet);
	
	PACKET_TDMA *packetTDMA = (PACKET_TDMA*)packet->payload;
	
	if (packetTDMA->payload[1])
	{
		ALOHA_Enable(false);
		return true;
	}
	else
	{
		TDMA_Enable(false);
		ALOHA_Enable(true);
		return false;
	}
	
}

void TDMA_SlotAllocationPacket(PACKET_Raw *packet)
{
	PACKET_TDMASlot *pTdma = (PACKET_TDMASlot*)packet->payload;
	
	TDMA_SecondSlot slot;
	slot.slot = pTdma->slotId;
	slot.len = pTdma->len;
	slot.lease = pTdma->lease;
	slot.enabled = true;
	
	TDMA_ConfigureSecondSlot(&slot);
	
	PACKET_Raw pRaw;
	pRaw.addr = 0;
	pRaw.type = PACKET_TDMA_ACK;
	pRaw.payload[0] = pTdma->seqNum;
	
	RADIO_Send((uint8_t*)&pRaw);
}

void TDMA_SendSlotEvent()
{
	PACKET_Raw pRaw;
	pRaw.addr = 0x00;
	pRaw.type = PACKET_EVENT;
	pRaw.payload[0] = 0xFF;
	pRaw.payload[1] = 0x00;
	char tmsg[255];
	sprintf(tmsg,"%i: Sending SLOT event\n",(int)TIMER_CounterGet(TIMER1));
	TRACE(tmsg);
	USB_Transmit((uint8_t*)&pRaw,32);
}

void TDMA_ConfigureSecondSlot(TDMA_SecondSlot *slot)
{
	memcpy((void*)&secondSlot,(void*)slot,sizeof(TDMA_SecondSlot));
	TDMA_EnableSecondSlot(secondSlot.enabled);
}

void TDMA_EnableSecondSlot(bool enable)
{
	if (enable)
	{
		TIMER_CompareSet(TIMER3, 0, (config.guardPeriod + config.transmitPeriod) * secondSlot.slot);
		TIMER_CompareSet(TIMER3, 1, (config.guardPeriod + config.transmitPeriod) * (secondSlot.slot + secondSlot.len) - config.protectionPeriod);
		TIMER_CompareSet(TIMER3, 2, (config.guardPeriod + config.transmitPeriod) * (secondSlot.slot + secondSlot.len) - 1);
		
		INT_Disable();
		TIMER_InitCC(TIMER3, 0, &timerCCCompare);
		TIMER_InitCC(TIMER3, 1, &timerCCCompare);
		TIMER_InitCC(TIMER3, 2, &timerCCCompare);
		
		TIMER_IntClear(TIMER3,TIMER_IF_CC0);
		TIMER_IntClear(TIMER3,TIMER_IF_CC1);
		TIMER_IntClear(TIMER3,TIMER_IF_CC2);
		
		TIMER_IntEnable(TIMER3,TIMER_IF_CC0);
		TIMER_IntEnable(TIMER3,TIMER_IF_CC1);
		TIMER_IntEnable(TIMER3,TIMER_IF_CC2);
		INT_Enable();
	}
	else
	{
		INT_Disable();
		TIMER_IntDisable(TIMER3,TIMER_IF_CC0);
		TIMER_IntDisable(TIMER3,TIMER_IF_CC1);
		TIMER_IntDisable(TIMER3,TIMER_IF_CC2);
		INT_Enable();
	}
	
	secondSlot.enabled = enable;
}

void TDMA_SetSecondSlotReloadCallback(uint32_t time)
{
	TIMER_CompareSet(TIMER3, 2, time);
}
