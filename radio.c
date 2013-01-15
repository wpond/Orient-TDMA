#include "radio.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "efm32_dma.h"
#include "efm32_int.h"
#include "efm32_gpio.h"
#include "efm32_usart.h"
#include "efm32_timer.h"

#include "nRF24L01.h"

#include "dma.h"
#include "led.h"
#include "queue.h"
#include "config.h"
#include "tdma_scheduler.h"

/* variables */
uint8_t NRF_CLEAR_IRQ = 0x70,
	RADIO_MODE_OFF = 0x0C,
	RADIO_MODE_RX = 0x0F,
	RADIO_MODE_TX = 0x0E;

DMA_CB_TypeDef radioCb = 
{
	.cbFunc = RADIO_TransferComplete,
	.userPtr = NULL,
};
DMA_DESCRIPTOR_TypeDef dmaTxBlock[2], 
	dmaRxBlock[2];

uint8_t radioScratch, radioDump;
bool transferActive = false;
RADIO_DmaTransfer activeTransfer;

RADIO_DmaTransfer transferQueueMemory[RADIO_TRANSFER_QUEUE_SIZE];
uint8_t rxQueueMemory[RADIO_RECV_QUEUE_SIZE * 32],
	txQueueMemory[RADIO_SEND_QUEUE_SIZE * 32];
queue_t transferQueue,
	rxQueue,
	txQueue;

bool systemCallsEnabled = false;
uint8_t fifoStatus = 0x21; // TX FULL & RX EMPTY

/* functions */
void RADIO_Init()
{
	
	currentMode = RADIO_OFF;
	systemCallActive = false;
	
	// configure queues
	QUEUE_Init(&transferQueue, (uint8_t*)transferQueueMemory, sizeof(RADIO_DmaTransfer), RADIO_TRANSFER_QUEUE_SIZE);
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
	USART_InitSync(USART0, &usartInit);
	USART0->ROUTE |=	USART_ROUTE_TXPEN | 
						USART_ROUTE_RXPEN | 
						USART_ROUTE_CLKPEN | 
						USART_ROUTE_LOCATION_LOC2;
	
	// configure radio
	uint8_t addr[5];
	memset(addr,0xE7,5);
	
	RADIO_WriteRegister(NRF_EN_AA, 0x00);
	RADIO_WriteRegister(NRF_EN_RXADDR, 0x3F);
	RADIO_WriteRegister(NRF_SETUP_AW, 0x03);
	RADIO_WriteRegister(NRF_SETUP_RETR, 0x00);
	RADIO_WriteRegister(NRF_RF_CH, NODE_CHANNEL);
	RADIO_WriteRegister(NRF_RF_SETUP, 0x07);
	
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
	
	RADIO_DmaTransfer transfer;
	
	transfer.ctrl = NRF_CONFIG | NRF_W_REGISTER;
	
	switch (mode)
	{
	case RADIO_OFF:
		transfer.src = &RADIO_MODE_OFF;
		break;
	case RADIO_TX:
		transfer.src = &RADIO_MODE_TX;
		break;
	case RADIO_RX:
		transfer.src = &RADIO_MODE_RX;
		NRF_CE_hi;
		NRF_RXEN_hi;
		break;
	default:
		return;
	}
	
	transfer.len = 1;
	transfer.dst = NULL;
	transfer.complete = NULL;
	transfer.systemCall = false;
	
	QUEUE_Empty(&transferQueue);
	
	RADIO_Flush(RADIO_TX);
	RADIO_Flush(RADIO_RX);
	RADIO_QueueTransfer(&transfer);
	
	currentMode = mode;
	
	if (mode == RADIO_TX)
	{
		RADIO_FifoCheckSetup();
	}
	
}

void RADIO_Flush(RADIO_Mode mode)
{
	
	RADIO_DmaTransfer transfer;
	
	switch (mode)
	{
	case RADIO_OFF:
		return;
	case RADIO_TX:
		transfer.ctrl = NRF_FLUSH_TX;
		break;
	case RADIO_RX:
		transfer.ctrl = NRF_FLUSH_RX;
		break;
	}
	
	transfer.len = 0;
	transfer.src = NULL;
	transfer.dst = NULL;
	transfer.complete = NULL;
	transfer.systemCall = false;
	
	RADIO_QueueTransfer(&transfer);
	
}

bool RADIO_Send(uint8_t packet[32])
{
	
	bool ret = QUEUE_Queue(&txQueue, packet);
	
	RADIO_FifoCheckSetup();
	
	return ret;
	
}

bool RADIO_Recv(uint8_t packet[32])
{
	
	RADIO_FifoCheckSetup();
	
	// check for lower level packets
	bool lowLevelPacket;
	do
	{
		
		lowLevelPacket = false;
		
		uint8_t *packetPeek = QUEUE_Peek(&rxQueue, false);
		
		if (packetPeek == NULL)
		{
			return false;
		}
		
		if (TDMA_IsEnabled() && TDMA_IsTimingPacket(packetPeek))
			lowLevelPacket = true;
		
		if (lowLevelPacket)
			QUEUE_Peek(&rxQueue, true);
		
	}
	while (lowLevelPacket);
	
	QUEUE_Dequeue(&rxQueue, packet);
	RADIO_FifoCheckSetup();
	
	return true;
	
}

uint8_t RADIO_ReadRegister(uint8_t reg)
{
	uint8_t val;
	RADIO_ReadRegisterMultiple(reg, &val, 1);
	return val;
}

void RADIO_WriteRegister(uint8_t reg, uint8_t val)
{
	RADIO_WriteRegisterMultiple(reg, &val, 1);
}

void RADIO_ReadRegisterMultiple(uint8_t reg, uint8_t *val, uint8_t len)
{
	
	RADIO_DmaTransfer transfer;
	bool complete = false;
	
	transfer.ctrl = reg | NRF_R_REGISTER;
	transfer.src = NULL;
	transfer.dst = val;
	transfer.len = len;
	transfer.complete = &complete;
	transfer.systemCall = false;
	
	RADIO_QueueTransfer(&transfer);
	
	while(!complete);
	
}

void RADIO_WriteRegisterMultiple(uint8_t reg, uint8_t *val, uint8_t len)
{
	
	RADIO_DmaTransfer transfer;
	bool complete = false;
	
	transfer.ctrl = reg | NRF_W_REGISTER;
	transfer.src = val;
	transfer.dst = NULL;
	transfer.len = len;
	transfer.complete = &complete;
	transfer.systemCall = false;
	
	RADIO_QueueTransfer(&transfer);
	
	while(!complete);
	
}

void RADIO_QueueTransfer(RADIO_DmaTransfer *transfer)
{
	
	if (!transferActive && QUEUE_IsEmpty(&transferQueue))
	{
		memcpy((void*)&activeTransfer, (void*)transfer, sizeof(RADIO_DmaTransfer));
		RADIO_TransferSetup(&activeTransfer);
	}
	else
	{
		while (!QUEUE_Queue(&transferQueue, (uint8_t*)transfer))
		{
			if (!transferActive)
				RADIO_TransferInit();
			// enter em1 ?
		}
		
		RADIO_TransferInit();
	}
	
}

void RADIO_TransferInit()
{
	
	if (!transferActive)
	{
		if (QUEUE_Dequeue(&transferQueue, (uint8_t*)&activeTransfer))
			RADIO_TransferSetup(&activeTransfer);
		else
			systemCallActive = false;
	}
	
}

void RADIO_TransferSetup(RADIO_DmaTransfer *transfer)
{
	
	if (transferActive)
		return;
	
	transferActive = true;
	
	if (transfer->systemCall)
	{
		switch (transfer->ctrl)
		{
		case NRF_R_RX_PAYLOAD:
			
			if (!RADIO_PacketDownloadSetup(transfer))
			{
				transferActive = false;
				RADIO_TransferInit();
				return;
			}
			
			break;
		case NRF_W_TX_PAYLOAD:
			
			if (!RADIO_PacketUploadSetup(transfer))
			{
				transferActive = false;
				RADIO_TransferInit();
				return;
			}
			
			break;
		default:
			break;
		}
	}
	
	DMA_CfgChannel_TypeDef	chnlCfg;
	DMA_CfgDescrSGAlt_TypeDef cfg;
	uint8_t scatter_count = 1;
	
	if (transfer->complete != NULL)
		*transfer->complete = false;
	
	radioCb.userPtr = transfer;
	
	
	// tx
	chnlCfg.highPri   = false;
	chnlCfg.enableInt = true;
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
	cfg.src		= (void *) &transfer->ctrl;	   
	cfg.dst		= (void *) &USART0->TXDATA; 
	cfg.nMinus1	= 0;
	DMA_CfgDescrScatterGather(dmaTxBlock, 0, &cfg);
	
	// tx data
	if (transfer->len > 0)
	{
		if (transfer->src == NULL)
		{
			cfg.srcInc	 = dmaDataIncNone;
			radioScratch = NRF_NOP;
			transfer->src = &radioScratch;
		}
		cfg.src		= (void *) transfer->src; 
		cfg.dst		= (void *) &USART0->TXDATA;
		cfg.nMinus1 = transfer->len - 1;
		DMA_CfgDescrScatterGather(dmaTxBlock, 1, &cfg);
	}
	
	// rx
	chnlCfg.highPri   = false;
	chnlCfg.enableInt = true;
	chnlCfg.select	= DMAREQ_USART0_RXDATAV;
	chnlCfg.cb		= &radioCb;
	DMA_CfgChannel(DMA_CHANNEL_RRX, &chnlCfg);
	
	cfg.srcInc	 = dmaDataIncNone;
	cfg.dstInc	 = dmaDataInc1;	 
	cfg.size	   = dmaDataSize1;	   
	cfg.arbRate	= dmaArbitrate1;
	cfg.hprot	  = 0;	  
	cfg.peripheral = true; 
	
	// rx ctrl
	cfg.src		= (void *) &USART0->RXDATA;	   
	cfg.dst		= (void *) &radioDump;
	cfg.nMinus1	= 0;
	DMA_CfgDescrScatterGather(dmaRxBlock, 0, &cfg);
	
	if (transfer->len > 0)
	{
		if (transfer->dst == NULL)
		{
			cfg.dstInc	 = dmaDataIncNone;
			transfer->dst = &radioScratch;
		}
		cfg.src		= (void *) &USART0->RXDATA;	   
		cfg.dst		= (void *) transfer->dst;
		cfg.nMinus1 = transfer->len - 1;
		DMA_CfgDescrScatterGather(dmaRxBlock, 1, &cfg);
		
		scatter_count = 2;
	}
	
	INT_Disable();
	
	NRF_CSN_lo;
	
	DMA_ActivateScatterGather(DMA_CHANNEL_RRX,
						false,
						dmaRxBlock,
						scatter_count);
	
	DMA_ActivateScatterGather(DMA_CHANNEL_RTX,
							false,
							dmaTxBlock,
							scatter_count);
	INT_Enable();
	
}

void RADIO_TransferComplete(unsigned int channel, bool primary, void *transfer)
{
	
	switch (channel)
	{
	case DMA_CHANNEL_RTX:
		// tx complete
		
		// do nothing - the transfer is complete when we have the last rx byte
		
		break;
	case DMA_CHANNEL_RRX:
		// rx complete
		NRF_CSN_hi;
		int i;
		for (i = 0; i < 2000; i++);
		RADIO_TransferTeardown((RADIO_DmaTransfer*)transfer);
		break;
	}
	
}

void RADIO_TransferTeardown(RADIO_DmaTransfer *transfer)
{
	
	if (transfer->complete != NULL)
	{
		*transfer->complete = true;
	}
	
	if (transfer->systemCall)
	{
		
		switch (transfer->ctrl)
		{
		case (NRF_FIFO_STATUS | NRF_R_REGISTER):
			
			RADIO_FifoCheckComplete();
			
			if (systemCallsEnabled)
			{
			
				switch (currentMode)
				{
					case RADIO_TX:
						
						if (!QUEUE_IsEmpty(&txQueue))
						{
						
							switch (fifoStatus & 0xF0)
							{
								case 0x10:
									RADIO_PacketUploadInit();
									//RADIO_PacketUploadInit();
									//RADIO_PacketUploadInit();
									systemCallActive = true;
									break;
								case 0x20:
									systemCallActive = false;
									break;
								default:
									RADIO_PacketUploadInit();
									systemCallActive = true;
									break;
							}
						
						}
						else
						{
							systemCallActive = false;
						}
						
						break;
					case RADIO_RX:
						
						if (!QUEUE_IsFull(&rxQueue))
						{
						
							switch (fifoStatus & 0x0F)
							{
								case 0x02:
									RADIO_PacketDownloadInit();
									RADIO_PacketDownloadInit();
									RADIO_PacketDownloadInit();
									RADIO_ClearIRQs();
									systemCallActive = true;
									break;
								case 0x01:
									systemCallActive = false;
									break;
								default:
									RADIO_PacketDownloadInit();
									RADIO_ClearIRQs();
									systemCallActive = true;
									break;
							}
						
						}
						else
						{
							systemCallActive = false;
						}
						
						break;
					default:
						systemCallActive = false;
						break;
				}
				
				if (systemCallActive)
				{
					// force queue a fifo check
					systemCallActive = false;
					RADIO_FifoCheckSetup();
					systemCallActive = true;
				}
			
			}
			else
			{
				systemCallActive = false;
			}
			
			break;
		case NRF_R_RX_PAYLOAD:
			RADIO_PacketDownloadComplete();
			break;
		case NRF_W_TX_PAYLOAD:
			RADIO_PacketUploadComplete();
			break;
		default:
			break;
		}
		
	}
	
	transferActive = false;
	RADIO_TransferInit();
	
}

void RADIO_PacketUploadInit()
{
	
	RADIO_DmaTransfer transfer;
	
	transfer.ctrl = NRF_W_TX_PAYLOAD;
	transfer.len = 32;
	transfer.src = NULL; // this is completed in setup
	transfer.dst = NULL;
	transfer.complete = NULL;
	transfer.systemCall = true;
	
	RADIO_QueueTransfer(&transfer);
	
}

bool RADIO_PacketUploadSetup(RADIO_DmaTransfer *transfer)
{
	
	transfer->src = QUEUE_Peek(&txQueue, false);
	
	if (transfer->src == NULL)
		return false;
	
	return true;
	
}

void RADIO_PacketUploadComplete()
{
	
	uint8_t *p = QUEUE_Peek(&txQueue, true);
	
	char tmsg[255];
	sprintf(tmsg, "%i: packet uploaded [0x%X]\n", TIMER_CounterGet(TIMER1),*p);
	TRACE(tmsg);
	
}

void RADIO_PacketDownloadInit()
{
	
	RADIO_DmaTransfer transfer;
	
	transfer.ctrl = NRF_R_RX_PAYLOAD;
	transfer.len = 32;
	transfer.src = NULL;
	transfer.dst = NULL; // this is completed in setup
	transfer.complete = NULL;
	transfer.systemCall = true;
	
	RADIO_QueueTransfer(&transfer);
	
}

bool RADIO_PacketDownloadSetup(RADIO_DmaTransfer *transfer)
{
	
	transfer->dst = QUEUE_Next(&rxQueue, false);
	
	if (transfer->dst == NULL)
		return false;
	
	return true;
	
}

void RADIO_PacketDownloadComplete()
{
	
	uint8_t *p = QUEUE_Next(&rxQueue, true);
	char tmsg[255];
	sprintf(tmsg,"%i: packet downloaded [0x%X]\n", TIMER_CounterGet(TIMER1), *p);
	TRACE(tmsg);
	
}

void RADIO_FifoCheckSetup()
{
	
	if (systemCallActive || !systemCallsEnabled || currentMode == RADIO_OFF)
		return;
	
	RADIO_DmaTransfer transfer;
	
	transfer.ctrl = NRF_FIFO_STATUS | NRF_R_REGISTER;
	transfer.len = 1;
	transfer.src = NULL;
	transfer.dst = &fifoStatus;
	transfer.complete = NULL;
	transfer.systemCall = true;
	
	char tmsg[255];
	sprintf(tmsg,"%i: Fifo check\n",TIMER_CounterGet(TIMER1));
	//TRACE(tmsg);
	
	systemCallActive = true;
	RADIO_QueueTransfer(&transfer);
	
	if (currentMode == RADIO_TX)
	{
		NRF_CE_lo;
	}
	
}

void RADIO_FifoCheckComplete()
{
	
	if (currentMode == RADIO_TX)
	{
		NRF_CE_hi;
	}
	
	systemCallActive = false;
	
}

void RADIO_IRQHandler()
{
	
	/*
	switch (currentMode)
	{
	case RADIO_TX:
		RADIO_PacketUploadInit();
		break;
	case RADIO_RX:
		RADIO_PacketDownloadInit();
		break;
	default:
		break;
	}
	*/
	
	TRACE("RADIO IRQ HANDLER\n");
	
	RADIO_FifoCheckSetup();
	RADIO_ClearIRQs();
	
}

void RADIO_ClearIRQs()
{
	RADIO_DmaTransfer transfer;
	
	transfer.ctrl = NRF_W_REGISTER | NRF_STATUS;
	transfer.len = 1;
	transfer.src = &NRF_CLEAR_IRQ;
	transfer.dst = NULL;
	transfer.complete = NULL;
	transfer.systemCall = false;
	
	RADIO_QueueTransfer(&transfer);
}

void RADIO_EnableSystemCalls(bool enable)
{
	systemCallsEnabled = enable;
	
	if (enable)
	{
		RADIO_FifoCheckSetup();
	}
}
