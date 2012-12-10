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

/* prototypes */
void RADIO_TransferComplete(unsigned int channel, bool primary, void *user);
void RADIO_TransferTeardown(RADIO_dmaTransfer *transfer);
bool RADIO_TransferSetup();
bool RADIO_Transfer(RADIO_dmaTransfer *transfer);
void RADIO_WriteRegister(uint8_t reg, uint8_t val);
void RADIO_ReadRegister(uint8_t reg, uint8_t *val);
bool RADIO_PacketSend(uint8_t offset);
void RADIO_PacketSendComplete();
bool RADIO_PacketRecv(uint8_t offset);
void RADIO_PacketRecvComplete();
void RADIO_TriggerAutoOperation();

/* variables */
static volatile bool activeTransfer = false;

uint8_t txQueue[RADIO_QUEUE_SIZE][32],
    txQueueReadPosition = RADIO_QUEUE_SIZE-1,
    txQueueWritePosition = 0,
    rxQueue[RADIO_QUEUE_SIZE][32],
    rxQueueReadPosition = RADIO_QUEUE_SIZE-1,
    rxQueueWritePosition = 0;

DMA_CB_TypeDef radioCb = 
{
    .cbFunc = RADIO_TransferComplete,
    .userPtr = NULL,
};
RADIO_dmaTransfer transferQueue[RADIO_TRANSFER_QUEUE_SIZE];
uint8_t transferQueueRead = RADIO_TRANSFER_QUEUE_SIZE-1,
    transferQueueWrite = 0;
DMA_DESCRIPTOR_TypeDef dmaTxBlock[2], 
    dmaRxBlock[2];
uint8_t radioScratch;

RADIO_Mode currentMode = RADIO_OFF;
bool autoOperation = false,
    activeAutoOperation = false;
uint8_t pendingAutoOperations = 0,
    last_fifo_status;

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
	
	RADIO_SetMode(RADIO_OFF);
	RADIO_SetAutoOperation(false);
	
	if ((status & 0x0F) == 0x0E)
	{
        TRACE("RADIO OK\n");
	}
	
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

void RADIO_IRQHandler()
{
    RADIO_WriteRegister(NRF_STATUS,0x70);
    RADIO_TriggerAutoOperation();
}

void RADIO_TriggerAutoOperation()
{
    
    if (activeAutoOperation || !autoOperation)
        return;
    
    // trigger a fifo status check
    RADIO_dmaTransfer transfer;
    
    transfer.ctrl = NRF_FIFO_STATUS | NRF_R_REGISTER;
    transfer.len = 1;
    transfer.src = &radioScratch;
    transfer.dest = &last_fifo_status;
    transfer.completePtr = NULL;
    transfer.transferType = RADIO_TRANSFER_FIFO_STATUS;
    
    if (RADIO_Transfer(&transfer))
        activeAutoOperation = true;
    
}

void RADIO_WriteRegister(uint8_t reg, uint8_t val)
{
    
    RADIO_dmaTransfer transfer;
    
    transfer.ctrl = reg | NRF_W_REGISTER;
    transfer.len = 1;
    transfer.transferScratch = val;
    transfer.src = &transfer.transferScratch;
    transfer.dest = &radioScratch;
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
    transfer.src = &radioScratch;
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
    
    if (transfer->completePtr != NULL)
        *transfer->completePtr = false;
    radioCb.userPtr = transfer;
    
    NRF_CSN_lo;
    activeTransfer = true;
    
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
    if (transfer->src == &radioScratch)
    {
        cfg.srcInc     = dmaDataIncNone;
        radioScratch = NRF_NOP;
    }
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
    cfg.dst        = (void *) &radioScratch;
    cfg.nMinus1    = 0;
    DMA_CfgDescrScatterGather(dmaRxBlock, 0, &cfg);
    
    if (transfer->dest == &radioScratch)
    {
        cfg.dstInc     = dmaDataIncNone;
    }
    cfg.src        = (void *) &USART0->RXDATA;       
    cfg.dst        = (void *) transfer->dest;
    cfg.nMinus1 = transfer->len - 1;
    DMA_CfgDescrScatterGather(dmaRxBlock, 1, &cfg);
    
    DMA_ActivateScatterGather(DMA_CHANNEL_RRX,
                        false,
                        dmaRxBlock,
                        2);
    
    DMA_ActivateScatterGather(DMA_CHANNEL_RTX,
                            false,
                            dmaTxBlock,
                            2);
    
    return true;
    
}

void RADIO_TransferTeardown(RADIO_dmaTransfer *transfer)
{
    
    // transfer complete, set csn high
    NRF_CSN_hi;
    activeTransfer = false;
    
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
                
                uint8_t status = last_fifo_status;
                pendingAutoOperations = 0;
                
                switch (currentMode)
                {
                case RADIO_TX:
                    if (status & 0x10)
                    {
                        // send 3 packets
                        for (int i = 0; i < 3; i++)
                            if (RADIO_PacketSend(i))
                                pendingAutoOperations++;
                            else
                                break;
                    }
                    else if (!(status & 0x20))
                    {
                        // send 1 packet
                        if (RADIO_PacketSend(0))
                            pendingAutoOperations++;
                    }
                    else
                    {
                        activeAutoOperation = false;
                        return;
                    }
                    
                    NRF_CE_lo; // <- REMOVE
                    
                    break;
                case RADIO_RX:
                    if (status & 0x02)
                    {
                        // recv 3 packets
                        for (int i = 0; i < 3; i++)
                            if (RADIO_PacketRecv(i))
                                pendingAutoOperations++;
                            else
                                break;
                    }
                    else if (!(status & 0x01))
                    {
                        // recv 1 packet
                        if (RADIO_PacketRecv(0))
                            pendingAutoOperations++;
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
                
                if (pendingAutoOperations == 0)
                {
                    activeAutoOperation = false;
                    return;
                }
                
            }
            break;
        case RADIO_TRANSFER_TX:
            
            // increment send pointer
            RADIO_PacketSendComplete();
            NRF_CE_hi; // <- REMOVE
            
            pendingAutoOperations--;
            
            if (pendingAutoOperations <= 0)
            {
                activeAutoOperation = false;
                RADIO_TriggerAutoOperation();
            }
            
            break;
        case RADIO_TRANSFER_RX:
            
            // increment recv pointer
            RADIO_PacketRecvComplete();
            
            pendingAutoOperations--;
            
            if (pendingAutoOperations <= 0)
            {
                activeAutoOperation = false;
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
        RADIO_TransferTeardown((RADIO_dmaTransfer*)transfer);
        RADIO_TransferSetup();
        break;
    }
    
}

bool RADIO_PacketSend(uint8_t offset)
{
    
    uint8_t readPosition = (txQueueReadPosition + offset) % RADIO_QUEUE_SIZE;
    
    if ((readPosition + 1) % RADIO_QUEUE_SIZE == txQueueWritePosition)
        return false;
    
    RADIO_dmaTransfer transfer;
    
    transfer.ctrl = NRF_W_TX_PAYLOAD;
    transfer.len = 32;
    transfer.src = txQueue[readPosition];
    transfer.dest = &radioScratch;
    transfer.completePtr = NULL;
    transfer.transferType = RADIO_TRANSFER_TX;
    
    return RADIO_Transfer(&transfer);
    
}

void RADIO_PacketSendComplete()
{
    txQueueReadPosition = (txQueueReadPosition + 1) % RADIO_QUEUE_SIZE;
}

bool RADIO_PacketRecv(uint8_t offset)
{
    
    uint8_t writePosition = (txQueueWritePosition + offset) % RADIO_QUEUE_SIZE;
    
    if (writePosition == txQueueReadPosition)
        return false;
    
    RADIO_dmaTransfer transfer;
    
    transfer.ctrl = NRF_R_RX_PAYLOAD;
    transfer.len = 32;
    transfer.src = &radioScratch;
    transfer.dest = txQueue[writePosition];
    transfer.completePtr = NULL;
    transfer.transferType = RADIO_TRANSFER_RX;
    
    return RADIO_Transfer(&transfer);
    
}

void RADIO_PacketRecvComplete()
{
    rxQueueWritePosition = (rxQueueWritePosition + 1) % RADIO_QUEUE_SIZE;
}

bool RADIO_Send(uint8_t *packet)
{
    
    if (txQueueWritePosition == txQueueReadPosition)
        return false;
    
    memcpy(txQueue[txQueueWritePosition],packet,32);
    
    txQueueWritePosition = (txQueueWritePosition + 1) % RADIO_QUEUE_SIZE;
    
    RADIO_TriggerAutoOperation();
    
    return true;
    
}

bool RADIO_Recv(uint8_t *packet)
{
    
    if ((rxQueueReadPosition + 1) % RADIO_QUEUE_SIZE == rxQueueWritePosition)
        return false;
    
    rxQueueReadPosition = (rxQueueReadPosition + 1) % RADIO_QUEUE_SIZE;
    
    memcpy(packet,rxQueue[rxQueueReadPosition],32);
    
    RADIO_TriggerAutoOperation();
    
    return true;
    
}
