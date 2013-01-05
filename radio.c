#include "radio.h"

#include <stdbool.h>
#include <stdint.h>

#include "efm32_dma.h"
#include "efm32_gpio.h"
#include "efm32_usart.h"

#include "nRF24L01.h"

#include "dma.h"
#include "led.h"

typedef struct
{
	uint8_t ctrl, *src, *dst;
	uint16_t len;
	bool *complete;
}
RADIO_DmaTransfer;

/* prototypes */
void RADIO_TransferSetup(RADIO_DmaTransfer *transfer);
void RADIO_TransferComplete(unsigned int channel, bool primary, void *transfer);
void RADIO_TransferTeardown(RADIO_DmaTransfer *transfer);
void RADIO_ReadRegisterMultiple(uint8_t reg, uint8_t *val, uint8_t len);
void RADIO_WriteRegisterMultiple(uint8_t reg, uint8_t *val, uint8_t len);
uint8_t RADIO_ReadRegister(uint8_t reg);
void RADIO_WriteRegister(uint8_t reg, uint8_t val);

/* variables */
DMA_CB_TypeDef radioCb = 
{
	.cbFunc = RADIO_TransferComplete,
	.userPtr = NULL,
};
DMA_DESCRIPTOR_TypeDef dmaTxBlock[2], 
	dmaRxBlock[2];

uint8_t radioScratch, radioDump;
bool activeTransfer = false;

/* functions */
void RADIO_Init()
{
	
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
	usartInit.baudrate = 7000000;
	USART_InitSync(USART0, &usartInit);
	USART0->ROUTE |=	USART_ROUTE_TXPEN | 
						USART_ROUTE_RXPEN | 
						USART_ROUTE_CLKPEN | 
						USART_ROUTE_LOCATION_LOC2;
	
	// configure radio
	RADIO_WriteRegister(NRF_EN_AA, 0x00);
	RADIO_WriteRegister(NRF_EN_RXADDR, 0x3F);
	RADIO_WriteRegister(NRF_SETUP_AW, 0x03);
	RADIO_WriteRegister(NRF_SETUP_RETR, 0x00);
	RADIO_WriteRegister(NRF_RF_CH, 2);
	RADIO_WriteRegister(NRF_RF_SETUP, 0x0F);

	RADIO_WriteRegister(NRF_RX_PW_P0, 32);
	
	RADIO_WriteRegister(NRF_DYNPD, 0x00);
	RADIO_WriteRegister(NRF_FEATURE, 0x00);
	
	GPIO_IntClear(1 << NRF_INT_PIN);
	GPIO_IntConfig(NRF_INT_PORT,NRF_INT_PIN,false,true,true);
	RADIO_WriteRegister(NRF_STATUS, 0x70);
	
	uint8_t status = RADIO_ReadRegister(NRF_STATUS);
	if ((status & 0x0F) == 0x0E)
	{
		LED_On(GREEN);
	}
	else
	{
		LED_On(RED);
	}
	
	while(1);
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
	
	RADIO_TransferSetup(&transfer);
	
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
	
	RADIO_TransferSetup(&transfer);
	
	while(!complete);
	
}

void RADIO_TransferSetup(RADIO_DmaTransfer *transfer)
{
	
	if (activeTransfer)
		return;
	
	DMA_CfgChannel_TypeDef	chnlCfg;
	DMA_CfgDescrSGAlt_TypeDef cfg;
	uint8_t scatter_count = 1;
	
	if (transfer->complete != NULL)
		*transfer->complete = false;
	radioCb.userPtr = transfer;
	
	NRF_CSN_lo;
	activeTransfer = true;
	
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
	
	DMA_ActivateScatterGather(DMA_CHANNEL_RRX,
						false,
						dmaRxBlock,
						scatter_count);
	
	DMA_ActivateScatterGather(DMA_CHANNEL_RTX,
							false,
							dmaTxBlock,
							scatter_count);
	
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
		RADIO_TransferTeardown((RADIO_DmaTransfer*)transfer);
		
		// set up next transfer ?
		
		break;
	}
	
}

void RADIO_TransferTeardown(RADIO_DmaTransfer *transfer)
{
	
	NRF_CSN_hi;
	activeTransfer = false;
	
	if (transfer->complete != NULL)
	{
		*transfer->complete = true;
	}
	
}