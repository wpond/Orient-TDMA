#include "radio.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "efm32_emu.h"
#include "efm32_rtc.h"
#include "efm32_int.h"
#include "efm32_gpio.h"
#include "efm32_timer.h"
#include "efm32_usart.h"
#include "efm32_dma.h"

#include "nRF24L01.h"

#include "dma.h"
#include "queue.h"
#include "config.h"
#include "packets.h"
#include "tdma.h"
#include "aloha.h"
#include "transport.h"
#include "eventmanager.h"

/* prototypes */
void RADIO_WriteRegister(uint8_t reg, uint8_t val);
uint8_t RADIO_ReadRegister(uint8_t reg);
void RADIO_WriteRegisterMultiple(uint8_t reg, uint8_t *data, uint8_t len);
void RADIO_ReadRegisterMultiple(uint8_t reg, uint8_t *data, uint8_t len);
void RADIO_Flush(RADIO_Mode mode);

bool RADIO_PacketDownload(uint8_t packet[32]);
bool RADIO_PacketUpload(uint8_t packet[32]);

void RADIO_TransferComplete(unsigned int channel, bool primary, void *transfer);

bool RADIO_QueueHelloResponse(PACKET_Raw *incomingPacket);

/* variables */
bool autoTransmitActive = false;

uint8_t rxQueueMemory[RADIO_RECV_QUEUE_SIZE * 32],
	txQueueMemory[RADIO_SEND_QUEUE_SIZE * 32];

DMA_CB_TypeDef radioCb = 
{
	.cbFunc = RADIO_TransferComplete,
	.userPtr = NULL,
};
DMA_DESCRIPTOR_TypeDef dmaTxBlock[2], 
	dmaRxBlock[2];

uint8_t scratch;
const uint8_t RADIO_NOP = NRF_NOP,
	RADIO_R_PAYLOAD = NRF_R_RX_PAYLOAD,
	RADIO_W_PAYLOAD = NRF_W_TX_PAYLOAD;

RADIO_Mode currentMode;

/* functions */
void RADIO_WriteRegister(uint8_t reg, uint8_t val)
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
	
	// wait for usart buffers to become clear
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	while (RADIO_USART->STATUS & USART_STATUS_RXDATAV)
		scratch = RADIO_USART->RXDATA;
	
	NRF_CSN_lo;
	
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	RADIO_USART->TXDATA = reg | NRF_W_REGISTER;
	while (!(RADIO_USART->STATUS & USART_STATUS_TXC));
	scratch = RADIO_USART->RXDATA;
	
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	RADIO_USART->TXDATA = val;
	while (!(RADIO_USART->STATUS & USART_STATUS_TXC));
	scratch = RADIO_USART->RXDATA;
	
	NRF_CSN_hi;
	
	transferActive = false;
	
}

uint8_t RADIO_ReadRegister(uint8_t reg)
{
	
	(void) scratch;
	
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
	
	// wait for usart buffers to become clear
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	while (RADIO_USART->STATUS & USART_STATUS_RXDATAV)
		scratch = RADIO_USART->RXDATA;
	
	NRF_CSN_lo;
	
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	RADIO_USART->TXDATA = reg | NRF_R_REGISTER;
	while (!(RADIO_USART->STATUS & USART_STATUS_TXC));
	scratch = RADIO_USART->RXDATA;
	
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	RADIO_USART->TXDATA = NRF_NOP;
	while (!(RADIO_USART->STATUS & USART_STATUS_TXC));
	scratch = RADIO_USART->RXDATA;
	
	NRF_CSN_hi;
	
	transferActive = false;
	
	return scratch;
	
}

void RADIO_WriteRegisterMultiple(uint8_t reg, uint8_t *data, uint8_t len)
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
	
	// wait for usart buffers to become clear
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	while (RADIO_USART->STATUS & USART_STATUS_RXDATAV)
		scratch = RADIO_USART->RXDATA;
	
	NRF_CSN_lo;
	
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	RADIO_USART->TXDATA = reg | NRF_W_REGISTER;
	while (!(RADIO_USART->STATUS & USART_STATUS_TXC));
	scratch = RADIO_USART->RXDATA;
	
	int i;
	for (i = 0; i < len; i++)
	{
	
		while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
		RADIO_USART->TXDATA = data[i];
		while (!(RADIO_USART->STATUS & USART_STATUS_TXC));
		scratch = RADIO_USART->RXDATA;
		
	}
	
	NRF_CSN_hi;
	
	transferActive = false;
	
}

void RADIO_ReadRegisterMultiple(uint8_t reg, uint8_t *data, uint8_t len)
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
	
	// wait for usart buffers to become clear
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	while (RADIO_USART->STATUS & USART_STATUS_RXDATAV)
		scratch = RADIO_USART->RXDATA;
	
	NRF_CSN_lo;
	
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	RADIO_USART->TXDATA = reg | NRF_W_REGISTER;
	while (!(RADIO_USART->STATUS & USART_STATUS_TXC));
	scratch = RADIO_USART->RXDATA;
	
	int i;
	for (i = 0; i < len; i++)
	{
	
		while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
		RADIO_USART->TXDATA = NRF_NOP;
		while (!(RADIO_USART->STATUS & USART_STATUS_TXC));
		data[i] = RADIO_USART->RXDATA;
		
	}
	
	NRF_CSN_hi;
	
	transferActive = false;
	
}

void RADIO_Init()
{
	
	transmitActive = false;
	transferActive = false;
	
	// configure queues
	QUEUE_Init(&rxQueue, (uint8_t*)rxQueueMemory, 32, RADIO_RECV_QUEUE_SIZE);
	QUEUE_Init(&txQueue, (uint8_t*)txQueueMemory, 32, RADIO_SEND_QUEUE_SIZE);
	
	// configure pins
	GPIO_PinModeSet(NRF_CE_PORT, NRF_CE_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(NRF_CSN_PORT, NRF_CSN_PIN, gpioModePushPull, 1);
	GPIO_PinModeSet(NRF_RXEN_PORT, NRF_RXEN_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(NRF_INT_PORT, NRF_INT_PIN, gpioModeInput, 0);

	GPIO_PinModeSet(gpioPortC, 11, gpioModePushPull, 1);
	GPIO_PinModeSet(gpioPortC, 10, gpioModeInput, 0);
	GPIO_PinModeSet(gpioPortC, 9, gpioModePushPull, 0);
	
	// configure usart
	USART_InitSync_TypeDef usartInit = USART_INITSYNC_DEFAULT;
	
	usartInit.msbf = true;
	usartInit.clockMode = usartClockMode0;
	usartInit.baudrate = 8000000;
	USART_InitSync(RADIO_USART, &usartInit);
	RADIO_USART->ROUTE |=	USART_ROUTE_TXPEN | 
						USART_ROUTE_RXPEN | 
						USART_ROUTE_CLKPEN | 
						RADIO_USART_LOCATION;
	
	// configure radio
	uint8_t addr[5];
	memset(addr,0xE7,5);
	
	RADIO_WriteRegister(NRF_EN_AA, 0x00);
	RADIO_WriteRegister(NRF_EN_RXADDR, 0x3F);
	RADIO_WriteRegister(NRF_SETUP_AW, 0x03);
	RADIO_WriteRegister(NRF_SETUP_RETR, 0x00);
	RADIO_WriteRegister(NRF_RF_CH, NODE_CHANNEL);
	RADIO_WriteRegister(NRF_RF_SETUP, 0x0F); // 1mbps (was at 2)
	
	RADIO_WriteRegisterMultiple(NRF_RX_ADDR_P0,addr,5);
	RADIO_WriteRegister(NRF_RX_PW_P0, 32);
	RADIO_WriteRegisterMultiple(NRF_RX_ADDR_P1,addr,5);
	RADIO_WriteRegister(NRF_RX_PW_P1, 32);
	RADIO_WriteRegisterMultiple(NRF_RX_ADDR_P2,addr,5);
	RADIO_WriteRegister(NRF_RX_PW_P2, 32);
	RADIO_WriteRegisterMultiple(NRF_RX_ADDR_P3,addr,5);
	RADIO_WriteRegister(NRF_RX_PW_P3, 32);
	RADIO_WriteRegisterMultiple(NRF_RX_ADDR_P4,addr,5);
	RADIO_WriteRegister(NRF_RX_PW_P4, 32);
	RADIO_WriteRegisterMultiple(NRF_RX_ADDR_P5,addr,5);
	RADIO_WriteRegister(NRF_RX_PW_P5, 32);
	
	RADIO_WriteRegister(NRF_DYNPD, 0x00);
	RADIO_WriteRegister(NRF_FEATURE, 0x00);
	
	RADIO_SetMode(RADIO_OFF);
	
	GPIO_IntClear(1 << NRF_INT_PIN);
	GPIO_IntConfig(NRF_INT_PORT,NRF_INT_PIN,false,true,true);
	RADIO_WriteRegister(NRF_STATUS, 0x70);
	
	uint8_t status = RADIO_ReadRegister(NRF_STATUS);
	if ((status & 0x0F) == 0x0E)
	{
		// status ok
	}
	
}

void RADIO_SetMode(RADIO_Mode mode)
{
	
	NRF_CE_lo;
	NRF_RXEN_lo;
	
	transmitActive = false;
	
	switch (mode)
	{
	case RADIO_OFF:
		RADIO_WriteRegister(NRF_CONFIG,mode);
		break;
	case RADIO_TX:
		RADIO_WriteRegister(NRF_CONFIG,mode);
		break;
	case RADIO_RX:
		RADIO_WriteRegister(NRF_CONFIG,mode);
		break;
	default:
		return;
	}
	
	currentMode = mode;
	
	RADIO_WriteRegister(NRF_STATUS,0xF0);
	RADIO_Flush(RADIO_TX);
	RADIO_Flush(RADIO_RX);
	
	if (mode == RADIO_RX)
	{
		NRF_CE_hi;
		NRF_RXEN_hi;
	}
	
}

void RADIO_Flush(RADIO_Mode mode)
{
	
	uint8_t cmd;
	
	switch (mode)
	{
	case RADIO_TX:
		cmd = NRF_FLUSH_TX;
		break;
	case RADIO_RX:
		cmd = NRF_FLUSH_RX;
		break;
	default:
		return;
	}
	
	uint8_t scratch;
	(void) scratch;
	
	// wait for usart buffers to become clear
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	while (RADIO_USART->STATUS & USART_STATUS_RXDATAV)
		scratch = RADIO_USART->RXDATA;
	
	NRF_CSN_lo;
	
	while (!(RADIO_USART->STATUS & USART_STATUS_TXBL));
	RADIO_USART->TXDATA = cmd;
	while (!(RADIO_USART->STATUS & USART_STATUS_TXC));
	scratch = RADIO_USART->RXDATA;
	
	NRF_CSN_hi;
	
}

bool RADIO_Send(uint8_t packet[32])
{
	if (QUEUE_Queue(&txQueue, packet))
	{
		if (currentMode == RADIO_TX && !transmitActive)
		{
			RADIO_PacketUpload((uint8_t*)QUEUE_Peek(&txQueue,false));
		}
		
		if (ALOHA_IsEnabled())
		{
			ALOHA_Send();
		}
		
		return true;
	}
	return false;
	
}

void RADIO_SetChannel(uint8_t ch)
{
	uint8_t mode = currentMode;
	RADIO_SetMode(RADIO_OFF);
	RADIO_WriteRegister(NRF_RF_CH,ch);
	RADIO_SetMode(mode);
	
	char tmsg[255];
	sprintf(tmsg,"Changed to channel %i\n", ch);
	TRACE(tmsg);
}

bool RADIO_SendDequeue(uint8_t packet[32])
{
	return QUEUE_Dequeue(&txQueue, packet);
}

bool RADIO_Recv(uint8_t packet[32])
{
	
	while (1)
	{
		
		if (QUEUE_IsEmpty(&rxQueue))
			return false;
		
		if (!RADIO_HandleIncomingPacket((PACKET_Raw*)QUEUE_Peek(&rxQueue,false)))
			return QUEUE_Dequeue(&rxQueue, packet);
		
		// all packets are passed back to the PC
		#ifdef BASESTATION
			return QUEUE_Dequeue(&rxQueue, packet);
		#endif
		
		QUEUE_Peek(&rxQueue,true);
		
	}
	
}

bool RADIO_HandleIncomingPacket(PACKET_Raw *packet)
{
	if (packet->addr == NODE_ID || packet->addr == BROADCAST_ID)
	{
		
		char tmsg[255];
		sprintf(tmsg,"MSG RECVD [addr=0x%X; type=0x%X]\n",packet->addr,packet->type);
		//TRACE(tmsg);
		
		switch (packet->type)
		{
		case PACKET_TDMA_TIMING:
			TDMA_TimingPacketReceived();
			return true;
			
		case PACKET_HELLO:
			RADIO_QueueHelloResponse(packet);
			return true;
			
		case PACKET_TDMA_CONFIG:
			if (TDMA_PacketConfigure(packet))
			{
				QUEUE_Empty(&rxQueue);
				TDMA_Enable(true);
			}
			return true;
			
		case PACKET_TDMA_ENABLE:
			if (TDMA_PacketEnable(packet))
			{
				QUEUE_Empty(&rxQueue);
				TDMA_Enable(true);
			}
			return true;
		
		case PACKET_TDMA_SLOT:
			TDMA_SlotAllocationPacket(packet);
			return true;
		
		case PACKET_EVENT:
		{
			uint8_t i;
			for (i = 0; i < 30; i++)
			{
				if (packet->payload[i] == 0)
					break;
				EVENT_Set(packet->payload[i]);
			}
			return true;
		}
		
		case PACKET_TRANSPORT_ACK:
		case PACKET_TRANSPORT_DATA:
			TRANSPORT_Recv(packet);
			return true;
		
		default:
			return false;
		}
	
	}
	else
	{
		return true;
	}
}

bool RADIO_PacketDownload(uint8_t packet[32])
{
	
	if (transferActive)
		return false;
	
	transferActive = true;
	
	DMA_CfgChannel_TypeDef	chnlCfg;
	DMA_CfgDescrSGAlt_TypeDef cfg;
	
	// tx
	chnlCfg.highPri   = false;
	chnlCfg.enableInt = false;
	chnlCfg.select	= DMAREQ_USART0_TXBL;
	chnlCfg.cb		= &radioCb;
	DMA_CfgChannel(DMA_CHANNEL_RTX, &chnlCfg);
	
	cfg.srcInc	 = dmaDataIncNone;
	cfg.dstInc	 = dmaDataIncNone;
	cfg.size	   = dmaDataSize1;	   
	cfg.arbRate	= dmaArbitrate1;
	cfg.hprot	  = 0;	  
	cfg.peripheral = true; 
	
	// tx ctrl
	cfg.src		= (void *) &RADIO_R_PAYLOAD; // download
	cfg.dst		= (void *) &USART0->TXDATA; 
	cfg.nMinus1	= 0;
	DMA_CfgDescrScatterGather(dmaTxBlock, 0, &cfg);
	
	// tx data
	cfg.src		= (void *) &RADIO_NOP; 
	cfg.dst		= (void *) &USART0->TXDATA;
	cfg.nMinus1 = 31;
	DMA_CfgDescrScatterGather(dmaTxBlock, 1, &cfg);
	
	// rx
	chnlCfg.highPri   = false;
	chnlCfg.enableInt = true;
	chnlCfg.select	= DMAREQ_USART0_RXDATAV;
	chnlCfg.cb		= &radioCb;
	DMA_CfgChannel(DMA_CHANNEL_RRX, &chnlCfg);
	
	cfg.srcInc	 = dmaDataIncNone;
	cfg.dstInc	 = dmaDataInc1;
	cfg.size	 = dmaDataSize1;	   
	cfg.arbRate	 = dmaArbitrate1;
	cfg.hprot	  = 0;	  
	cfg.peripheral = true; 
	
	// rx ctrl
	cfg.src		= (void *) &USART0->RXDATA;	   
	cfg.dst		= (void *) &scratch;
	cfg.nMinus1	= 0;
	DMA_CfgDescrScatterGather(dmaRxBlock, 0, &cfg);
	
	cfg.src		= (void *) &USART0->RXDATA;	   
	cfg.dst		= (void *) packet;
	cfg.nMinus1 = 31;
	DMA_CfgDescrScatterGather(dmaRxBlock, 1, &cfg);
	
	INT_Disable();
	
	NRF_CSN_lo;
	
	DMA_ActivateScatterGather(DMA_CHANNEL_RRX,
						false,
						dmaRxBlock,
						2);
	
	DMA_ActivateScatterGather(DMA_CHANNEL_RTX,
							false,
							dmaTxBlock,
							2);
	
	INT_Enable();
	
	return true;
	
}

bool RADIO_PacketUpload(uint8_t packet[32])
{
	
	if (transferActive)
		return false;
	
	if (packet[1] == PACKET_TRANSPORT_ACK)
	{
		PACKET_Raw pRaw;
		pRaw.addr = 0x00;
		pRaw.type = PACKET_EVENT;
		pRaw.payload[0] = 0xFE;
		pRaw.payload[1] = 0x00;
		USB_Transmit((uint8_t*)&pRaw,32);
		
		char tmsg[255];
		sprintf(tmsg,"%i: sending ACK packet [%i %i]\n\n",(int)TIMER_CounterGet(TIMER1),(int)packet[0],(int)packet[1]);
		TRACE(tmsg);
	}
	
	if (packet[1] == PACKET_TRANSPORT_DATA)
	{
		char tmsg[255];
		sprintf(tmsg,"%i: sending DATA packet %i/%i\n",(int)TIMER_CounterGet(TIMER1),(int)packet[4],(int)packet[3]);
		//TRACE(tmsg);
	}
	
	
	transferActive = true;
	transmitActive = true;
	
	DMA_CfgChannel_TypeDef	chnlCfg;
	DMA_CfgDescrSGAlt_TypeDef cfg;
	
	// tx
	chnlCfg.highPri   = false;
	chnlCfg.enableInt = false;
	chnlCfg.select	= DMAREQ_USART0_TXBL;
	chnlCfg.cb		= &radioCb;
	DMA_CfgChannel(DMA_CHANNEL_RTX, &chnlCfg);
	
	cfg.srcInc	 = dmaDataInc1;
	cfg.dstInc	 = dmaDataIncNone;
	cfg.size	   = dmaDataSize1;	   
	cfg.arbRate	= dmaArbitrate1;
	cfg.hprot	  = 0;	  
	cfg.peripheral = true; 
	
	// tx ctrl
	cfg.src		= (void *) &RADIO_W_PAYLOAD; // download
	cfg.dst		= (void *) &USART0->TXDATA; 
	cfg.nMinus1	= 0;
	DMA_CfgDescrScatterGather(dmaTxBlock, 0, &cfg);
	
	// tx data
	cfg.src		= (void *) packet; 
	cfg.dst		= (void *) &USART0->TXDATA;
	cfg.nMinus1 = 31;
	DMA_CfgDescrScatterGather(dmaTxBlock, 1, &cfg);
	
	// rx
	chnlCfg.highPri   = false;
	chnlCfg.enableInt = true;
	chnlCfg.select	= DMAREQ_USART0_RXDATAV;
	chnlCfg.cb		= &radioCb;
	DMA_CfgChannel(DMA_CHANNEL_RRX, &chnlCfg);
	
	cfg.srcInc	 = dmaDataIncNone;
	cfg.dstInc	 = dmaDataIncNone;
	cfg.size	 = dmaDataSize1;	   
	cfg.arbRate	 = dmaArbitrate1;
	cfg.hprot	  = 0;	  
	cfg.peripheral = true; 
	
	// rx ctrl
	cfg.src		= (void *) &USART0->RXDATA;	   
	cfg.dst		= (void *) &scratch;
	cfg.nMinus1	= 0;
	DMA_CfgDescrScatterGather(dmaRxBlock, 0, &cfg);
	
	cfg.src		= (void *) &USART0->RXDATA;	   
	cfg.dst		= (void *) &scratch;
	cfg.nMinus1 = 31;
	DMA_CfgDescrScatterGather(dmaRxBlock, 1, &cfg);
	
	INT_Disable();
	
	NRF_CSN_lo;
	
	DMA_ActivateScatterGather(DMA_CHANNEL_RRX,
						false,
						dmaRxBlock,
						2);
	
	DMA_ActivateScatterGather(DMA_CHANNEL_RTX,
							false,
							dmaTxBlock,
							2);
	
	INT_Enable();
	
	return true;
	
}

void RADIO_TransferComplete(unsigned int channel, bool primary, void *transfer)
{
	
	switch (channel)
	{
	case DMA_CHANNEL_RTX:
		// tx complete
		
		// do nothing - the transfer is only complete when we have the last rx byte
		
		break;
	case DMA_CHANNEL_RRX:
		// rx complete
		NRF_CSN_hi;
		
		transferActive = false;
		
		switch(currentMode)
		{
		case RADIO_OFF:
			break;
		case RADIO_TX:
			NRF_CE_hi;
			QUEUE_Peek(&txQueue,true);
			break;
		case RADIO_RX:
			QUEUE_Next(&rxQueue,true);
			
			uint8_t fifoStatus = RADIO_ReadRegister(NRF_FIFO_STATUS);
			if (!(fifoStatus & 0x01))
			{
				RADIO_PacketDownload((uint8_t*)QUEUE_Next(&rxQueue,false));
			}
			
			break;
		}
		
		break;
	}
	
}

void RADIO_IRQHandler() 
{
	
	uint8_t status = RADIO_ReadRegister(NRF_STATUS);
	RADIO_WriteRegister(NRF_STATUS,status);
	uint8_t fifoStatus = RADIO_ReadRegister(NRF_FIFO_STATUS);
	switch(status & 0xF0)
	{
	case 0x10:
		break;
	case 0x20:
		NRF_CE_lo;
		if (autoTransmitActive && (fifoStatus & 0x10) && !QUEUE_IsEmpty(&txQueue))
		{
			RADIO_PacketUpload((uint8_t*)QUEUE_Peek(&txQueue,false));
			transmitActive = true;
		}
		else
		{
			transmitActive = false;
		}
		//TRACE("PACKET SENT\n");
		break;
	case 0x40:
		if (!QUEUE_IsFull(&rxQueue))
		{
			if (!(fifoStatus & 0x01))
			{
				lastPacketReceived = RTC_CounterGet();
				RADIO_PacketDownload((uint8_t*)QUEUE_Next(&rxQueue,false));
			}
		}
		break;
	}
	
	//TRACE("RADIO IRQ\n");
	
}

void RADIO_EnableAutoTransmit(bool enable)
{
	autoTransmitActive = enable;
	
	if (autoTransmitActive && 
			!transmitActive &&
			currentMode == RADIO_TX && 
			!QUEUE_IsEmpty(&txQueue) &&
			(RADIO_ReadRegister(NRF_FIFO_STATUS) & 0x10))
	{
		RADIO_PacketUpload((uint8_t*)QUEUE_Peek(&txQueue,false));
	}
}

bool RADIO_QueueHelloResponse(PACKET_Raw *incomingPacket)
{
	
	PACKET_PayloadHello *hello = (PACKET_PayloadHello*)incomingPacket->payload;
	
	char tmsg[255];
	sprintf(tmsg,"Challenge/Response: 0x%X\n",hello->challengeResponse);
	TRACE(tmsg);
	
	if (hello->challengeResponse != HELLO_CHALLENGE)
		return true;
	
	incomingPacket->addr = BASESTATION_ID;
	hello->challengeResponse = NODE_ID;
	
	return RADIO_Send((uint8_t*)incomingPacket);
	
}
