#include "radio.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "efm32_int.h"
#include "efm32_timer.h"
#include "efm32_gpio.h"

#include "nRF24L01.h"

#include "config.h"
#include "led.h"
#include "queue.h"
#include "usb.h"
#include "packets.h"
#include "alloc.h"

typedef enum
{
	RADIO_STATE_OFF,
	
	RADIO_STATE_MASTER_OF,
	RADIO_STATE_MASTER_TIM1_CC0,
	RADIO_STATE_MASTER_TIM1_CC1,
	RADIO_STATE_MASTER_TIM1_CC2,
	
	RADIO_STATE_SLAVE_OF,
	RADIO_STATE_SLAVE_TIM0_CC0,
	RADIO_STATE_SLAVE_TIM0_CC1,
	RADIO_STATE_SLAVE_TIM0_CC2,
	RADIO_STATE_SLAVE_TIM1_CC0,
	RADIO_STATE_SLAVE_TIM1_CC1,
	RADIO_STATE_SLAVE_TIM1_CC2,
	
	RADIO_STATE_SLAVE_TIM3_CC0,
	RADIO_STATE_SLAVE_TIM3_CC1,
	RADIO_STATE_SLAVE_TIM3_CC2,
	
	RADIO_STATE_SLAVE_SYNC_INIT,
	RADIO_STATE_SLAVE_SYNC,
	RADIO_STATE_SLAVE_SYNC_COMPLETE,
	
}
RADIO_State;

typedef struct
{
	bool enabled,
		slotRequest;
	uint8_t lastFrame;
	uint8_t lastSeg;
	uint8_t lastFlags;
}
RADIO_DataState;

/* prototypes */
static void RADIO_WriteRegister(uint8_t reg, uint8_t val);
static uint8_t RADIO_ReadRegister(uint8_t reg);
static void RADIO_WriteRegisterMultiple(uint8_t reg, const uint8_t *data, uint8_t len);
static void RADIO_ReadRegisterMultiple(uint8_t reg, uint8_t *data, uint8_t len);
static void RADIO_Flush(RADIO_Mode mode);
static void RADIO_SetState(RADIO_State _state);

// utils 
static void RADIO_SafeIncrement(uint8_t volatile *i);
static void RADIO_SafeDecrement(uint8_t volatile *i);
static void RADIO_TimingUpdate(char* msg);
static RADIO_State RADIO_GetState();

/* variables */
static uint8_t scratch,
	dataFrameId = 0,
	dataSendPos = 0;
static RADIO_Mode currentMode;
static uint8_t txQueueMemory[RADIO_SEND_QUEUE_SIZE * 32],
	dataQueueMemory[RADIO_DATA_SIZE * 32];
static queue_t txQueue,
		dataQueue;
static volatile uint8_t irqCount = 0;
static RADIO_TDMAConfig config =
{
	.master = false,
	.channel = 102,
	.slot = 0,
	.slotCount = 10,
	.guardPeriod = 100,
	.transmitPeriod = 900,
	.protectionPeriod = 50,
};
static volatile RADIO_State state = RADIO_STATE_OFF;
static volatile bool stateChanged = false,
	sendPackets = true,
	syncNextPacket = false,
	syncd = false;
static uint8_t timingPacket[32];
static uint32_t lastIrq = 0xFFFFFFFF,
	countSinceSync = 0;
static bool tdmaEnabled = false;
static RADIO_DataState nodeStates[255];
static uint8_t lastNodeAckd = 0,
	secondSlotId = 0,
	secondSlotLease = 0,
	secondSlotLen = 0;
static uint32_t timerOverflows = 0;

static TIMER_Init_TypeDef timerInit =
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

static TIMER_InitCC_TypeDef timerCCOff = 
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

static TIMER_InitCC_TypeDef timerCCCompare = 
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

static TIMER_InitCC_TypeDef timerCCCe = 
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

static TIMER_InitCC_TypeDef timerCCIrq = 
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

/* functions */
void RADIO_Init()
{
	
	int i;
	for (i = 0; i < 256; i++)
	{
		nodeStates[i].enabled = false;
	}
	
	#ifdef BASESTATION
		ALLOC_Init(5,4);
	#endif
	
	QUEUE_Init(&txQueue, (uint8_t*)txQueueMemory, 32, RADIO_SEND_QUEUE_SIZE);
	QUEUE_Init(&dataQueue, (uint8_t*)dataQueueMemory, 32, RADIO_DATA_SIZE);
	
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
	RADIO_WriteRegister(NRF_RF_SETUP, 0x0F);
	
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
		LED_On(GREEN);
	}
	
	// configure timers
	memset(timingPacket,0,32);
	RADIO_DisableTDMA();
	RADIO_SetMode(RADIO_RX);
	
}

static void RADIO_TimingUpdate(char* msg)
{
	static char tmsg[255];
	sprintf(tmsg,"%i[%i]: %s\n",(int)TIMER_CounterGet(TIMER0),(int)timerOverflows,msg);
	TRACE(tmsg);
}

static RADIO_State RADIO_GetState()
{
	RADIO_State _state;
	INT_Disable();
	_state = state;
	INT_Enable();
	return _state;
}

void RADIO_SetMode(RADIO_Mode mode)
{
	
	NRF_CE_lo;
	NRF_RXEN_lo;
	
	if (!(mode == RADIO_RX || mode == RADIO_TX || mode == RADIO_OFF))
		return;
	
	RADIO_WriteRegister(NRF_CONFIG,mode);
	
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

static void RADIO_WriteRegister(uint8_t reg, uint8_t val)
{
	
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
	
}

static uint8_t RADIO_ReadRegister(uint8_t reg)
{
	
	(void) scratch;
	
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
	
	return scratch;
	
}

static void RADIO_WriteRegisterMultiple(uint8_t reg, const uint8_t *data, uint8_t len)
{
	
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
	
}

static void RADIO_ReadRegisterMultiple(uint8_t reg, uint8_t *data, uint8_t len)
{
	
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
	
}

static void RADIO_Flush(RADIO_Mode mode)
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

void RADIO_IRQHandler()
{
	if (currentMode == RADIO_TX)
	{
		NRF_CE_lo;
	}
	RADIO_SafeIncrement(&irqCount);
	RADIO_WriteRegister(NRF_STATUS,0xF0);
	lastIrq = TIMER_CounterGet(TIMER1);
}

uint16_t count = 0;
void RADIO_Main()
{
	
	static uint8_t packet[32];
	
	if (stateChanged)
	{
		
		INT_Disable();
		RADIO_State _state = state;
		stateChanged = false;
		INT_Enable();
		
		switch (_state)
		{
		case RADIO_STATE_OFF:
			RADIO_SetMode(RADIO_OFF);
			RADIO_TimingUpdate("radio off");
			break;
		case RADIO_STATE_MASTER_OF:
			sendPackets = false;
			lastNodeAckd = 0;
			RADIO_SetMode(RADIO_TX);
			TIMER_InitCC(TIMER1, 0, &timerCCCe);
			RADIO_WriteRegisterMultiple(NRF_W_TX_PAYLOAD,timingPacket,32);
			char msg[32];
			sprintf(msg,"last recv count = %i",count);
			count = 0;
			RADIO_TimingUpdate(msg);
			break;
		case RADIO_STATE_MASTER_TIM1_CC0:
			sendPackets = true;
			RADIO_TimingUpdate("send timing packet");
			break;
		case RADIO_STATE_MASTER_TIM1_CC1:
			sendPackets = false;
			RADIO_TimingUpdate("protection period start");
			break;
		case RADIO_STATE_MASTER_TIM1_CC2:
			RADIO_SetMode(RADIO_RX);
			RADIO_TimingUpdate("receiving");
			break;
		case RADIO_STATE_SLAVE_OF:
			RADIO_SetMode(RADIO_RX);
			syncNextPacket = true;
			countSinceSync++;
			dataSendPos = 0; // reset send offset
			if (secondSlotLease > 0)
			{
				secondSlotLease--;
				if (secondSlotLease == 0)
				{
					TIMER_IntDisable(TIMER3,TIMER_IF_CC0);
					TIMER_IntDisable(TIMER3,TIMER_IF_CC1);
					TIMER_IntDisable(TIMER3,TIMER_IF_CC2);
				}
			}
			sprintf(msg,"last send count = %i",count);
			count = 0;
			RADIO_TimingUpdate(msg);
			break;
		case RADIO_STATE_SLAVE_TIM0_CC0:
			RADIO_SetMode(RADIO_OFF);
			RADIO_TimingUpdate("radio off");
			break;
		case RADIO_STATE_SLAVE_TIM3_CC0:
			TIMER_CompareSet(TIMER1, 0, config.guardPeriod + ((config.guardPeriod + config.transmitPeriod) * secondSlotId));
		case RADIO_STATE_SLAVE_TIM0_CC1:
			sendPackets = false;
			RADIO_SetMode(RADIO_TX);
			TIMER_InitCC(TIMER1, 0, &timerCCCe);
			if (!QUEUE_IsEmpty(&txQueue))
			{
				if (QUEUE_Dequeue(&txQueue,(void*)packet))
				{
					RADIO_WriteRegisterMultiple(NRF_W_TX_PAYLOAD,packet,32);
				}
			}
			else
			{
				uint8_t *packetPtr = QUEUE_Get(&dataQueue,dataSendPos++);
				if (packetPtr != NULL)
				{
					RADIO_WriteRegisterMultiple(NRF_W_TX_PAYLOAD,packetPtr,32);
				}
			}
			RADIO_TimingUpdate("queuing first packet");
			break;
		case RADIO_STATE_SLAVE_TIM1_CC0:
			sendPackets = true;
			RADIO_TimingUpdate("sending packets");
			break;
		case RADIO_STATE_SLAVE_TIM3_CC1:
		case RADIO_STATE_SLAVE_TIM1_CC1:
			sendPackets = false;
			RADIO_TimingUpdate("protection period start");
			break;
		case RADIO_STATE_SLAVE_TIM3_CC2:
			TIMER_CompareSet(TIMER1, 0, config.guardPeriod + ((config.guardPeriod + config.transmitPeriod) * config.slot));
		case RADIO_STATE_SLAVE_TIM1_CC2:
			RADIO_SetMode(RADIO_OFF);
			RADIO_TimingUpdate("end transmission period");
			break;
		case RADIO_STATE_SLAVE_SYNC_INIT:
		{
			// check radio queue for sync packet
			bool syncFound = false;
			
			if (countSinceSync == 0)
			{
				uint32_t t = config.guardPeriod + (TIMER_CounterGet(TIMER1) - lastIrq);
						
				TIMER_CounterSet(TIMER0,t);
				TIMER_CounterSet(TIMER1,t);
				TIMER_CounterSet(TIMER3,t);
				
				syncFound = true;
				
				TIMER_IntEnable(TIMER1,TIMER_IF_OF);
			}
			
			if (!syncFound)
			{
				// by resetting this state, we'll check next iteration
				RADIO_SetState(RADIO_STATE_SLAVE_SYNC_INIT);
			}
			else
			{
				RADIO_TimingUpdate("timing packet found");
				RADIO_SetState(RADIO_STATE_SLAVE_SYNC);
			}
			
			break;
		}
		case RADIO_STATE_SLAVE_SYNC:
			break;
		case RADIO_STATE_SLAVE_SYNC_COMPLETE:
			
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
			
			RADIO_TimingUpdate("sync complete\n");
			syncd = true;
			break;
		}
		
	}
	
	if (!tdmaEnabled)
	{
		if (currentMode == RADIO_RX && !QUEUE_IsEmpty(&txQueue))
		{
			wait(*(uint32_t*)(0xFE081F0) & 0x0000000F);
			RADIO_SetMode(RADIO_TX);
			sendPackets = true;
			RADIO_TimingUpdate("aloha tx mode");
		}
		else if (currentMode == RADIO_TX && QUEUE_IsEmpty(&txQueue) && (RADIO_ReadRegister(NRF_FIFO_STATUS) & 0x10))
		{
			RADIO_SetMode(RADIO_RX);
			sendPackets = false;
			RADIO_TimingUpdate("aloha rx mode");
		}
	}
	
	INT_Disable();
	uint8_t _irqCount = irqCount;
	INT_Enable();
	
	uint8_t *packetPtr;
	if (_irqCount > 0)
	{
	
		uint8_t fifoStatus = RADIO_ReadRegister(NRF_FIFO_STATUS);
		
		RADIO_SafeDecrement(&irqCount);
		
		if (sendPackets && currentMode == RADIO_TX && (!QUEUE_IsEmpty(&txQueue) || !QUEUE_IsEmpty(&dataQueue) || lastNodeAckd < 255))
		{
			 
			 if (fifoStatus & 0x10)
			 {
				packetPtr = NULL;
				if (QUEUE_Dequeue(&txQueue,(void*)packet))
				{
					packetPtr = packet;
				}
				else
				{
					packetPtr = QUEUE_Get(&dataQueue,dataSendPos++);
				}
				
				#ifdef BASESTATION
					if (packetPtr == NULL)
					{
						if (lastNodeAckd < 255)
						{
							
							while (++lastNodeAckd < 255)
							{
								if (nodeStates[lastNodeAckd].enabled)
								{
									packet[0] = lastNodeAckd;
									packet[1] = PACKET_TRANSPORT_ACK;
									packet[2] = nodeStates[lastNodeAckd].lastFrame;
									packet[3] = nodeStates[lastNodeAckd].lastSeg;
									packet[4] = nodeStates[lastNodeAckd].lastFlags;
									if (nodeStates[lastNodeAckd].slotRequest)
									{
										ALLOC_Request(lastNodeAckd);
									}
									uint8_t slotId,
										len,
										lease,
										ack;
									if (ALLOC_CheckAndDecrement(lastNodeAckd,&slotId,&len,&lease,&ack))
									{
										packet[5] = 1;
										packet[6] = ack;
										packet[7] = slotId;
										packet[8] = len;
										packet[9] = lease;
										
										static char msg[64];
										sprintf(msg,"lease: [nid=%i,slotid=%i,len=%i,lease=%i,ack=%i]",lastNodeAckd,slotId,len,lease,ack);
										RADIO_TimingUpdate(msg);
									}
									else
									{
										packet[5] = 0;
									}
									packetPtr = packet;
									
									static char msg[32];
									sprintf(msg,"send ack n:%i %i/%i",(int)lastNodeAckd,(int)packet[3],(int)packet[2]);
									RADIO_TimingUpdate(msg);
									
									nodeStates[lastNodeAckd].slotRequest = false;
									break;
								}
							}
							
						}
					}
				#endif
				
				if (packetPtr != NULL)
				{
					RADIO_WriteRegisterMultiple(NRF_W_TX_PAYLOAD,packetPtr,32);
					count++;
				}
				
			 }
			 
			 NRF_CE_hi;
			 
		}
		else if (currentMode == RADIO_RX)
		{
			while (!(fifoStatus & 0x01))
			{
				RADIO_ReadRegisterMultiple(NRF_R_RX_PAYLOAD,packet,32);
				
				if (fifoStatus & 0x02)
				{
					LED_On(BLUE);
				}
				
				#ifdef BASESTATION
					if (packet[1] == PACKET_TRANSPORT_DATA)
					{
						
						if (nodeStates[packet[2]].enabled)
						{
							uint8_t nextSeg = nodeStates[packet[2]].lastSeg + 1,
								nextFrame = nodeStates[packet[2]].lastFrame;
							if (nodeStates[packet[2]].lastFlags & TRANSPORT_FLAG_SEGMENT_END)
							{
								nextFrame++;
								nextSeg = 0;
							}
							
							if (packet[4] == nextSeg && packet[3] == nextFrame)
							{
								nodeStates[packet[2]].lastSeg = packet[4];
								nodeStates[packet[2]].lastFrame = packet[3];
								nodeStates[packet[2]].lastFlags = packet[6];
								nodeStates[packet[2]].slotRequest |= (nodeStates[packet[2]].lastFlags & TRANSPORT_FLAG_SLOT_REQUEST);
							}
							else
							{
								static char msg[32];
								sprintf(msg,"got %i/%i expecting %i/%i",packet[4],packet[3],nextSeg,nextFrame);
								RADIO_TimingUpdate(msg);
							}
						}
						else
						{
							nodeStates[packet[2]].enabled = true;
							nodeStates[packet[2]].slotRequest = false;
							nodeStates[packet[2]].lastSeg = packet[4];
							nodeStates[packet[2]].lastFrame = packet[3];
							nodeStates[packet[2]].lastFlags = packet[6];
						}
						
					}
					//USB_Transmit(packet,32);
				#else
					RADIO_TimingUpdate("packet received");
					if (packet[0] == NODE_ID || packet[0] == BROADCAST_ID || packet[1] == 0)
					{
						switch (packet[1])
						{
						case PACKET_TDMA_TIMING:
							countSinceSync = 0;
							RADIO_TimingUpdate("sync packet found");
							break;
						case PACKET_HELLO:
							if (packet[2] == 0xFF)
							{
								packet[2] = NODE_ID;
								RADIO_Send(packet);
							}
							break;
						case PACKET_TDMA_CONFIG:
						{
							PACKET_TDMA *packetTDMA = (PACKET_TDMA*)&packet[2];
							RADIO_TDMAConfig *c = (RADIO_TDMAConfig*)packetTDMA->payload;
							RADIO_ConfigTDMA(c);
							if (packetTDMA->payload[sizeof(RADIO_TDMAConfig)])
							{
								RADIO_EnableTDMA();
							}
							else
							{
								RADIO_DisableTDMA();
							}
							break;
						}
						case PACKET_TDMA_ENABLE:
							if (packet[3])
							{
								RADIO_EnableTDMA();
							}
							else
							{
								RADIO_DisableTDMA();
							}
						case PACKET_TDMA_SLOT:
							break;
						case PACKET_TDMA_ACK:
							break;
						case PACKET_TRANSPORT_DATA:
							break;
						case PACKET_TRANSPORT_ACK:
						{
							uint8_t *p = QUEUE_Peek(&dataQueue,false),
								nextFrame = packet[2],
								nextSeg = packet[3] + 1;
							uint16_t sent = 0;
							
							if (packet[4] & TRANSPORT_FLAG_SEGMENT_END)
							{
								nextFrame++;
								nextSeg = 0;
							}
							
							static char msg[32];
							
							sprintf(msg,"fast forward to %i/%i [%i]",nextSeg,nextFrame,packet[6]);
							RADIO_TimingUpdate(msg);
							
							while (p != NULL)
							{
								
								if (p[3] == nextFrame && p[4] == nextSeg)
								{
									break;
								}
								
								sent++;
								QUEUE_Peek(&dataQueue,true); // fast dequeue
								p = QUEUE_Peek(&dataQueue,false);
								
							}
							
							sprintf(msg,"sent %i packets %i remaining [ack %i/%i]",sent,QUEUE_Count(&dataQueue),packet[3],packet[2]);
							RADIO_TimingUpdate(msg);
							
							// second slot config
							if (packet[5])
							{
								secondSlotId = packet[7];
								secondSlotLen = packet[8];
								secondSlotLease = packet[9];
								
								TIMER_CompareSet(TIMER3, 0, (config.guardPeriod + config.transmitPeriod) * secondSlotId);
								TIMER_CompareSet(TIMER3, 1, ((config.guardPeriod + config.transmitPeriod) * (secondSlotId + secondSlotLen))  - config.protectionPeriod);
								TIMER_CompareSet(TIMER3, 2, ((config.guardPeriod + config.transmitPeriod) * (secondSlotId + secondSlotLen))  - 1);

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
							
							break;
						}
						case PACKET_EVENT:
						{
							uint8_t e, p = 2;
							for (p = 2; p < 32; p++)
							{
								e = packet[p];
								if (e == 0)
									break;
								static char msg[32];
								sprintf(msg,"event 0x%2.2X",e);
								RADIO_TimingUpdate(msg);
							}
						}
							break;
						}
					}
				#endif
				
				count++;
				fifoStatus = RADIO_ReadRegister(NRF_FIFO_STATUS);
			 }
		}
		/*
		static char help[32];
		sprintf(help,"irqCount = %i[%2.2X]",_irqCount,RADIO_ReadRegister(NRF_STATUS));
		RADIO_TimingUpdate(help);
		*/
	}
	
	if (tdmaEnabled && countSinceSync > 3)
	{
		RADIO_DisableTDMA();
		RADIO_TimingUpdate("tdma out of sync");
		RADIO_EnableTDMA();
	}
	
}

bool RADIO_Send(const uint8_t packet[32])
{
	RADIO_SafeIncrement(&irqCount);
	return QUEUE_Queue(&txQueue,(void*)packet);
}

RADIO_Mode RADIO_GetMode()
{
	return currentMode;
}

static void RADIO_SafeIncrement(uint8_t volatile *i)
{
	INT_Disable();
	(*i)++;
	INT_Enable();
}

static void RADIO_SafeDecrement(uint8_t volatile *i)
{
	INT_Disable();
	(*i)--;
	INT_Enable();
}

void RADIO_EnableTDMA()
{
	
	TIMER_Reset(TIMER0);
	TIMER_Reset(TIMER1);
	TIMER_Reset(TIMER3);
	
	NVIC_DisableIRQ(GPIO_EVEN_IRQn);
	
	RADIO_SetMode(RADIO_OFF);
	RADIO_WriteRegister(NRF_RF_CH,config.channel);
	
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
		
		TIMER_CounterSet(TIMER0,top-1);
		TIMER_CounterSet(TIMER1,top-1);
		TIMER_CounterSet(TIMER3,top-1);
		
	}
	else
	{
		
		// enable timing capture mode
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
		
		TIMER_CompareSet(TIMER0, 0, config.guardPeriod + config.transmitPeriod);
		
		syncd = false;
		
		RADIO_SetMode(RADIO_RX);
		RADIO_SetState(RADIO_STATE_SLAVE_SYNC_INIT);
		
		countSinceSync = 0;
		
	}
	
	tdmaEnabled = true;
	RADIO_TimingUpdate("tdma started");
	
}

void RADIO_DisableTDMA()
{
	TIMER_Reset(TIMER0);
	TIMER_Reset(TIMER1);
	TIMER_Reset(TIMER3);
	
	NVIC_EnableIRQ(GPIO_EVEN_IRQn);
	
	RADIO_WriteRegister(NRF_RF_CH,NODE_CHANNEL);
	tdmaEnabled = false;
	
	RADIO_SetMode(RADIO_RX);
}

void RADIO_ConfigTDMA(RADIO_TDMAConfig *_config)
{
	memcpy(&config,_config,sizeof(RADIO_TDMAConfig));
}

static void RADIO_SetState(RADIO_State _state)
{
	INT_Disable();
	state = _state;
	stateChanged = true;
	INT_Enable();
}

void TIMER0_IRQHandler()
{
	
	uint32_t flags = TIMER_IntGet(TIMER0);
	
	if (flags & TIMER_IF_CC2)
	{
		uint32_t time = TIMER_CaptureGet(TIMER0,2);
		
		if (!config.master && syncNextPacket)
		{
			// sync timers
			INT_Disable();
			time = TIMER_CounterGet(TIMER0) - (time - (config.guardPeriod + 14));
			
			while (time < 0)
				time += TIMER_TopGet(TIMER0); 
			
			TIMER_CounterSet(TIMER0, time);
			TIMER_CounterSet(TIMER1, time);
			TIMER_CounterSet(TIMER3, time);
			
			INT_Enable();
			
			static char msg[32];
			sprintf(msg,"sync timers[%i]",time);
			//RADIO_TimingUpdate(msg);
			// enable full slave mode
			if (!syncd)
			{
				RADIO_SetState(RADIO_STATE_SLAVE_SYNC_COMPLETE);
			}
			syncNextPacket = false;
		}
		
		RADIO_IRQHandler();
	}
	
	if (flags & TIMER_IF_CC0)
	{
		RADIO_SetState(RADIO_STATE_SLAVE_TIM0_CC0);
	}
	
	if (flags & TIMER_IF_CC1)
	{
		RADIO_SetState(RADIO_STATE_SLAVE_TIM0_CC1);
	}
	
	TIMER_IntClear(TIMER0,flags);
	
}

void TIMER1_IRQHandler()
{
	
	uint32_t flags = TIMER_IntGet(TIMER1);
	
	if (flags & TIMER_IF_OF)
	{
		timerOverflows++;
		if (config.master)
		{
			RADIO_SetState(RADIO_STATE_MASTER_OF);
		}
		else
		{
			RADIO_SetState(RADIO_STATE_SLAVE_OF);
		}
	}
	if (flags & TIMER_IF_CC0)
	{
		if (config.master)
		{
			TIMER_InitCC(TIMER1, 0, &timerCCOff);
			RADIO_SetState(RADIO_STATE_MASTER_TIM1_CC0);
		}
		else
		{
			TIMER_InitCC(TIMER1, 0, &timerCCOff);
			RADIO_SetState(RADIO_STATE_SLAVE_TIM1_CC0);
		}
	}
	if (flags & TIMER_IF_CC1)
	{
		if (config.master)
		{
			RADIO_SetState(RADIO_STATE_MASTER_TIM1_CC1);
		}
		else
		{
			RADIO_SetState(RADIO_STATE_SLAVE_TIM1_CC1);
		}
	}
	if (flags & TIMER_IF_CC2)
	{
		if (config.master)
		{
			RADIO_SetState(RADIO_STATE_MASTER_TIM1_CC2);
		}
		else
		{
			RADIO_SetState(RADIO_STATE_SLAVE_TIM1_CC2);
		}
	}
	
	TIMER_IntClear(TIMER1,flags);
	
}

void TIMER3_IRQHandler()
{
	
	uint32_t flags = TIMER_IntGet(TIMER3);
	
	if (flags & TIMER_IF_CC0)
	{
		RADIO_SetState(RADIO_STATE_SLAVE_TIM3_CC0);
	}
	if (flags & TIMER_IF_CC1)
	{
		RADIO_SetState(RADIO_STATE_SLAVE_TIM3_CC1);
	}
	if (flags & TIMER_IF_CC2)
	{
		RADIO_SetState(RADIO_STATE_SLAVE_TIM3_CC2);
	}
	
	TIMER_IntClear(TIMER3,flags);
	
}

bool RADIO_SendData(const uint8_t *data, uint16_t len)
{
	
	//if (RADIO_DATA_SIZE - QUEUE_Count(&dataQueue) < len/25)
	if (QUEUE_Count(&dataQueue) >= 1000)
		return false;
	
	// move to data queue
	static uint8_t packet[32];
	
	packet[0] = BASESTATION_ID;
	packet[1] = PACKET_TRANSPORT_DATA;
	packet[2] = NODE_ID;
	packet[3] = dataFrameId++;
	
	// flags
	packet[6] = TRANSPORT_FLAG_SLOT_REQUEST;
	
	uint8_t segId = 0;
	
	while (len > 0)
	{
		
		packet[4] = segId++;
		
		if (len > 25)
		{
			memcpy(&packet[7],data,25);
			data += 25;
			len -= 25;
			packet[5] = 25;
		}
		else
		{
			memcpy(&packet[7],data,len);
			packet[5] = len;
			packet[6] |= TRANSPORT_FLAG_SEGMENT_END;
			len = 0;
		}
		
		QUEUE_Queue(&dataQueue,packet);
		
	}
	
	RADIO_SafeIncrement(irqCount);
	return true;
	
}
