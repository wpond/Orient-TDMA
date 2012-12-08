#include "radio.h"

#include <stdlib.h>

#include "efm32_gpio.h"
#include "efm32_usart.h"

#include "config.h"
#include "dma.h"
#include "led.h"

#include "nRF24L01.h"

/* structs */
typedef enum
{
    RADIO_TRANSFER_MANUAL,
    RADIO_TRANSFER_FIFO_STATUS,
    RADIO_TRANSFER_RX,
    RADIO_TRANSFER_TX
} RADIO_TransferType;

typedef struct
{
    uint8_t ctrl,
        len,
        transferScratch;
    uint8_t *src;
    uint8_t *dest;
    bool *completePtr;
    RADIO_TransferType transferType;
} RADIO_dmaTransfer;

typedef enum
{
    RADIO_OFF,
    RADIO_TX,
    RADIO_RX
} RADIO_Mode;

/* prototypes */
void RADIO_TransferComplete(unsigned int channel, bool primary, void *user);
void RADIO_TransferTeardown(RADIO_dmaTransfer *transfer);
bool RADIO_TransferSetup();
bool RADIO_Transfer(RADIO_dmaTransfer *transfer);
void RADIO_WriteRegister(uint8_t reg, uint8_t val);
void RADIO_ReadRegister(uint8_t reg, uint8_t *val);
bool RADIO_SetMode(RADIO_Mode mode);
void RADIO_SetAutoOperation(bool enable);

/* variables */
static volatile bool activeTransfer = false;
uint8_t txQueue[32 * RADIO_QUEUE_SIZE],
    txQueuePosition = 0,
    rxQueue[32 * RADIO_QUEUE_SIZE],
    rxQueuePosition = 0;
DMA_CB_TypeDef radioCb = 
{
    .cbFunc = RADIO_TransferComplete,
    .userPtr = NULL,
};
RADIO_dmaTransfer transferQueue[RADIO_TRANSFER_QUEUE_SIZE];
uint8_t transferQueueRead = RADIO_TRANSFER_QUEUE_SIZE-1,
    transferQueueWrite = 0;
DMA_DESCRIPTOR_TypeDef dmaTxBlock[2], dmaRxBlock[2];
uint8_t dmaScratch;
RADIO_Mode currentMode = RADIO_OFF;
bool autoOperation = false,
    activeAutoOperation = false;
uint8_t pendingAutoOperations = 0;

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
	USART0->ROUTE |=    USART_ROUTE_TXPEN | 
                        USART_ROUTE_RXPEN | 
                        USART_ROUTE_CLKPEN | 
                        USART_ROUTE_LOCATION_LOC2;
	
	// configure radio
	RADIO_WriteRegister(NRF_STATUS, 0x70);
	GPIO_IntClear(1 << NRF_INT_PIN);
	GPIO_IntConfig(NRF_INT_PORT,NRF_INT_PIN,false,true,true);

	RADIO_WriteRegister(NRF_EN_AA, 0x00);
	RADIO_WriteRegister(NRF_EN_RXADDR, 0x3F);
	RADIO_WriteRegister(NRF_SETUP_AW, 0x03);
	RADIO_WriteRegister(NRF_SETUP_RETR, 0x00);
	RADIO_WriteRegister(NRF_RF_CH, 102);
	RADIO_WriteRegister(NRF_RF_SETUP, 0x0F);

	RADIO_WriteRegister(NRF_RX_PW_P0, 32);
    
	RADIO_WriteRegister(NRF_DYNPD, 0x00);
	RADIO_WriteRegister(NRF_FEATURE, 0x00);
	
	uint8_t status;
	RADIO_ReadRegister(NRF_STATUS,&status);
	
	if ((status & 0x0F) == 0x0E)
	{
        TRACE("RADIO OK\n");
        LED_On(BLUE);
	}
	
	RADIO_SetMode(RADIO_OFF);
	RADIO_SetAutoOperation(true);
	
}

bool RADIO_SetMode(RADIO_Mode mode)
{
    uint8_t val;
    
    if (mode == currentMode)
        return true;
    
    switch (mode)
    {
    case RADIO_OFF:
        val = 0x0C;
        break;
    case RADIO_TX:
        val = 0x0E;
        break;
    case RADIO_RX:
        val = 0x0F;
        break;
    default:
        return false;
    }
    
    RADIO_WriteRegister(NRF_CONFIG, val);
    currentMode = mode;
    
    return true;
    
}

void RADIO_SetAutoOperation(bool enable)
{
    autoOperation = enable;
}

void RADIO_TriggerAutoOperation()
{
    
    if (activeAutoOperation || !autoOperation)
        return;
    
    // trigger a fifo status check
    RADIO_dmaTransfer transfer;
    
    transfer.ctrl = NRF_FIFO_STATUS | NRF_R_REGISTER;
    transfer.len = 1;
    transfer.transferScratch = NRF_NOP;
    transfer.src = &transfer.transferScratch;
    transfer.dest = &transfer.transferScratch;
    transfer.completePtr = NULL;
    transfer.transferType = RADIO_TRANSFER_FIFO_STATUS;
    
    RADIO_Transfer(&transfer);
    
}

void RADIO_WriteRegister(uint8_t reg, uint8_t val)
{
    
    RADIO_dmaTransfer transfer;
    
    transfer.ctrl = reg | NRF_W_REGISTER;
    transfer.len = 1;
    transfer.transferScratch = val;
    transfer.src = &transfer.transferScratch;
    transfer.dest = &dmaScratch;
    transfer.completePtr = NULL;
    transfer.transferType = RADIO_TRANSFER_MANUAL;
    
    while (!RADIO_Transfer(&transfer));
    
}

void RADIO_ReadRegister(uint8_t reg, uint8_t *val)
{
    
    RADIO_dmaTransfer transfer;
    bool complete = false;
    
    transfer.ctrl = reg | NRF_R_REGISTER;
    transfer.len = 1;
    transfer.transferScratch = NRF_NOP;
    transfer.src = &transfer.transferScratch;
    transfer.dest = &transfer.transferScratch;
    transfer.completePtr = &complete;
    transfer.transferType = RADIO_TRANSFER_MANUAL;
    
    while (!RADIO_Transfer(&transfer));
    
    while (!complete);
    
    *val = transfer.transferScratch;
    
}

bool RADIO_Transfer(RADIO_dmaTransfer *transfer)
{
    
    if (transferQueueWrite == transferQueueRead)
        return false;
    
    memcpy(&transferQueue[transferQueueWrite], transfer, sizeof(RADIO_dmaTransfer));
    transferQueueWrite = (transferQueueWrite + 1) % RADIO_TRANSFER_QUEUE_SIZE;
    
    RADIO_TransferSetup();
    
    return true;
}

bool RADIO_TransferSetup()
{
    
    if (activeTransfer || (transferQueueRead + 1) % RADIO_TRANSFER_QUEUE_SIZE == transferQueueWrite)
        return false;
    
    DMA_CfgChannel_TypeDef    chnlCfg;
    DMA_CfgDescrSGAlt_TypeDef cfg;
    
    transferQueueRead = (transferQueueRead + 1) % RADIO_TRANSFER_QUEUE_SIZE;
    RADIO_dmaTransfer *transfer = &transferQueue[transferQueueRead];
    
    *transfer->completePtr = false;
    radioCb.userPtr = transfer;
    
    // tx
    chnlCfg.highPri   = false;
    chnlCfg.enableInt = true;
    chnlCfg.select    = DMAREQ_USART0_TXBL;
    chnlCfg.cb        = &radioCb;
    DMA_CfgChannel(DMA_CHANNEL_RTX, &chnlCfg);
    
    cfg.srcInc     = dmaDataInc1;
    cfg.dstInc     = dmaDataIncNone;     
    cfg.size       = dmaDataSize1;       
    cfg.arbRate    = dmaArbitrate1;
    cfg.hprot      = 0;      
    cfg.peripheral = true; 
    
    // tx ctrl
    cfg.src        = (void *) &transfer->ctrl;       
    cfg.dst        = (void *) &USART0->TXDATA; 
    cfg.nMinus1    = 0;
    DMA_CfgDescrScatterGather(dmaTxBlock, 0, &cfg);
    
    // tx data
    cfg.src        = (void *) transfer->src;       
    cfg.dst        = (void *) &USART0->TXDATA;
    cfg.nMinus1 = transfer->len - 1;
    DMA_CfgDescrScatterGather(dmaTxBlock, 1, &cfg);
    
    // rx
    chnlCfg.highPri   = false;
    chnlCfg.enableInt = true;
    chnlCfg.select    = DMAREQ_USART0_RXDATAV;
    chnlCfg.cb        = &radioCb;
    DMA_CfgChannel(DMA_CHANNEL_RRX, &chnlCfg);
    
    cfg.srcInc     = dmaDataIncNone;
    cfg.dstInc     = dmaDataInc1;     
    cfg.size       = dmaDataSize1;       
    cfg.arbRate    = dmaArbitrate1;
    cfg.hprot      = 0;      
    cfg.peripheral = true; 
    
    // rx ctrl
    cfg.src        = (void *) &USART0->RXDATA;       
    cfg.dst        = (void *) &dmaScratch;
    cfg.nMinus1    = 0;
    DMA_CfgDescrScatterGather(dmaRxBlock, 0, &cfg);
    
    cfg.src        = (void *) &USART0->RXDATA;       
    cfg.dst        = (void *) transfer->dest;
    cfg.nMinus1 = transfer->len - 1;
    DMA_CfgDescrScatterGather(dmaRxBlock, 1, &cfg);
    
    NRF_CSN_lo;
    activeTransfer = true;
    DMA_ActivateScatterGather(DMA_CHANNEL_RTX,
                            false,
                            dmaTxBlock,
                            2);
    DMA_ActivateScatterGather(DMA_CHANNEL_RRX,
                            false,
                            dmaRxBlock,
                            2);
    
    return true;
    
}

void RADIO_TransferTeardown(RADIO_dmaTransfer *transfer)
{
    
    // transfer complete, set csn high
    NRF_CSN_hi;
    
    if (transfer->transferType != RADIO_TRANSFER_MANUAL && activeAutoOperation)
    {
        
        // cease auto operation
        if (autoOperation == false)
        {
            activeAutoOperation = false;
            pendingAutoOperations = 0;
            return;
        }
        
        switch (transfer->transferType)
        {
        case RADIO_TRANSFER_FIFO_STATUS:
            {
                
                uint8_t status = transfer->transferScratch;
                
                switch (currentMode)
                {
                case RADIO_TX:
                    if (status & 0x10)
                    {
                        // send 3 packets
                        pendingAutoOperations = 3;
                    }
                    else if (!(status & 0x20))
                    {
                        // send 1 packet
                        pendingAutoOperations = 1;
                    }
                    else
                    {
                        activeAutoOperation = false;
                        return;
                    }
                    break;
                case RADIO_RX:
                    if (status & 0x02)
                    {
                        // recv 3 packets
                        pendingAutoOperations = 3;
                    }
                    else if (!(status & 0x01))
                    {
                        // recv 1 packet
                        pendingAutoOperations = 1;
                    }
                    else
                    {
                        activeAutoOperation = false;
                        return;
                    }
                    break;
                default:
                    activeAutoOperation = false;
                    return;
                }
            
            }
            break;
        case RADIO_TRANSFER_TX:
            
            // increment send pointer
            
            pendingAutoOperations--;
            
            if (pendingAutoOperations == 0)
            {
                RADIO_TriggerAutoOperation();
            }
            
            break;
        case RADIO_TRANSFER_RX:
            
            // increment recv pointer
            
            pendingAutoOperations--;
            
            if (pendingAutoOperations == 0)
            {
                RADIO_TriggerAutoOperation();
            }
            
            break;
        case RADIO_TRANSFER_MANUAL:
            break;
        }
        
    }
    else if (transfer->transferType == RADIO_TRANSFER_MANUAL)
    {
    
        // notify transfer complete
        if (transfer->completePtr != NULL)
        {
            *transfer->completePtr = true;
        }
        
    }
    
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
        activeTransfer = false;
        RADIO_TransferTeardown((RADIO_dmaTransfer*)transfer);
        RADIO_TransferSetup();
        break;
    }
    
}

bool RADIO_Send(uint8_t *packet)
{
    return false;
}

bool RADIO_Recv(uint8_t *packet)
{
    return false;
}
