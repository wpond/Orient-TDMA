#include "radio.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "efm32_int.h"
#include "efm32_timer.h"

#include "nRF24L01.h"

#include "led.h"
#include "queue.h"
#include "usb.h"
#include "packets.h"

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
	
	RADIO_STATE_SLAVE_SYNC_INIT,
	RADIO_STATE_SLAVE_SYNC,
	RADIO_STATE_SLAVE_SYNC_COMPLETE,
	
}
RADIO_State;

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
static uint8_t scratch;
static RADIO_Mode currentMode;
static uint8_t rxQueueMemory[RADIO_RECV_QUEUE_SIZE * 32],
	txQueueMemory[RADIO_SEND_QUEUE_SIZE * 32];
static queue_t rxQueue,
		txQueue;
static volatile uint8_t irqCount = 0;
static RADIO_TDMAConfig config =
{
	.master = false,
	.channel = 102,
	.slot = 1,
	.slotCount = 10,
	.guardPeriod = 100,
	.transmitPeriod = 900,
	.protectionPeriod = 50,
};
static volatile RADIO_State state = RADIO_STATE_OFF;
static volatile bool stateChanged = false,
	sendPackets = true,
	syncNextPacket = false;
static uint8_t timingPacket[32];

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
	
	QUEUE_Init(&rxQueue, (uint8_t*)rxQueueMemory, 32, RADIO_RECV_QUEUE_SIZE);
	QUEUE_Init(&txQueue, (uint8_t*)txQueueMemory, 32, RADIO_SEND_QUEUE_SIZE);
	
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
	RADIO_WriteRegister(NRF_RF_SETUP, 0x1F); // 1mbps (was at 2)
	
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
	
}

static void RADIO_TimingUpdate(char* msg)
{
	static char tmsg[255];
	sprintf(tmsg,"%i: %s\n",(int)TIMER_CounterGet(TIMER0),msg);
	USB_Transmit((uint8_t*)tmsg,(uint8_t)strlen(tmsg));
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
	RADIO_TimingUpdate("RADIO_IRQHandler");
	RADIO_WriteRegister(NRF_STATUS,0xF0);
	LED_Toggle(RED);
}

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
			RADIO_SetMode(RADIO_TX);
			TIMER_InitCC(TIMER1, 0, &timerCCCe);
			RADIO_WriteRegisterMultiple(NRF_W_TX_PAYLOAD,timingPacket,32);
			RADIO_TimingUpdate("of");
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
			RADIO_TimingUpdate("of");
			break;
		case RADIO_STATE_SLAVE_TIM0_CC0:
			RADIO_SetMode(RADIO_OFF);
			RADIO_TimingUpdate("radio off");
			break;
		case RADIO_STATE_SLAVE_TIM0_CC1:
			sendPackets = false;
			RADIO_SetMode(RADIO_TX);
			TIMER_InitCC(TIMER1, 0, &timerCCCe);
			if (!QUEUE_IsEmpty(&txQueue))
			{
				QUEUE_Dequeue(&txQueue,(void*)packet);
				RADIO_WriteRegisterMultiple(NRF_W_TX_PAYLOAD,packet,32);
			}
			RADIO_TimingUpdate("queuing first packet");
			break;
		case RADIO_STATE_SLAVE_TIM1_CC0:
			sendPackets = true;
			RADIO_TimingUpdate("sending packets");
			break;
		case RADIO_STATE_SLAVE_TIM1_CC1:
			sendPackets = false;
			RADIO_TimingUpdate("protection period start");
			break;
		case RADIO_STATE_SLAVE_TIM1_CC2:
			RADIO_SetMode(RADIO_OFF);
			RADIO_TimingUpdate("end transmission period");
			break;
		case RADIO_STATE_SLAVE_SYNC_INIT:
		{
			// check radio queue for sync packet
			bool syncFound = false;
			
			while (!QUEUE_IsEmpty(&rxQueue))
			{
				uint8_t *packet;
				if ((packet = QUEUE_Peek(&rxQueue,false)) != NULL)
				{
					if (packet[1] == PACKET_TDMA_TIMING)
					{
						uint32_t t = TIMER_TopGet(TIMER0) - config.guardPeriod;
						
						TIMER_CounterSet(TIMER0,t);
						TIMER_CounterSet(TIMER1,t);
						TIMER_CounterSet(TIMER3,t);
						
						TIMER_IntEnable(TIMER0,TIMER_IF_CC0);
						TIMER_IntEnable(TIMER1,TIMER_IF_OF);
						
						syncFound = true;
					}
				}
				
				QUEUE_Peek(&rxQueue,true);
				
			}
			
			if (!syncFound)
			{
				// by resetting this state, we'll check next iteration
				RADIO_SetState(RADIO_STATE_SLAVE_SYNC_INIT);
			}
			else
			{
				RADIO_TimingUpdate("timing packet found");
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
			break;
		}
		
	}
	
	INT_Disable();
	uint8_t _irqCount = irqCount;
	INT_Enable();
	
	if (_irqCount > 0)
	{
	
		uint8_t	fifoStatus;
		
		RADIO_SafeDecrement(&irqCount);
		
		if (sendPackets && currentMode == RADIO_TX && !QUEUE_IsEmpty(&txQueue))
		{
			 fifoStatus = RADIO_ReadRegister(NRF_FIFO_STATUS);
			 
			 if (fifoStatus & 0x10 && !QUEUE_IsEmpty(&txQueue))
			 {
				QUEUE_Dequeue(&txQueue,(void*)packet);
				RADIO_WriteRegisterMultiple(NRF_W_TX_PAYLOAD,packet,32);
				fifoStatus = RADIO_ReadRegister(NRF_FIFO_STATUS);
			 }
			 
			 NRF_CE_hi;
			 
		}
		else if (currentMode == RADIO_RX && !QUEUE_IsFull(&rxQueue))
		{
			fifoStatus = RADIO_ReadRegister(NRF_FIFO_STATUS);
			 while (!(fifoStatus & 0x01) && !QUEUE_IsFull(&rxQueue))
			 {
				RADIO_ReadRegisterMultiple(NRF_R_RX_PAYLOAD,packet,32);
				QUEUE_Queue(&rxQueue,(void*)packet);
				
				if (((PACKET_Raw*)packet)->type == PACKET_TDMA_TIMING)
				{
					
				}
				
				fifoStatus = RADIO_ReadRegister(NRF_FIFO_STATUS);
			 }
		}
		static char help[32];
		sprintf(help,"irqCount = %i[%2.2X]",_irqCount,RADIO_ReadRegister(NRF_STATUS));
		RADIO_TimingUpdate(help);
		
	}
	
}

bool RADIO_Send(const uint8_t packet[32])
{
	RADIO_SafeIncrement(&irqCount);
	RADIO_TimingUpdate("RADIO_Send");
	return QUEUE_Queue(&txQueue,(void*)packet);
}

// improve using QUEUE_Peek 
bool RADIO_Recv(uint8_t packet[32])
{
	return QUEUE_Dequeue(&rxQueue,(void*)packet);
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
		
		RADIO_SetMode(RADIO_RX);
		RADIO_SetState(RADIO_STATE_SLAVE_SYNC_INIT);
		
	}
	
	RADIO_TimingUpdate("tdma started");
	
}

void RADIO_DisableTDMA()
{
	TIMER_Reset(TIMER0);
	TIMER_Reset(TIMER1);
	TIMER_Reset(TIMER3);
	
	NVIC_EnableIRQ(GPIO_EVEN_IRQn);
	
	RADIO_WriteRegister(NRF_RF_CH,NODE_CHANNEL);
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
		if (!config.master && syncNextPacket)
		{
			// sync timers
			
			// enable full slave mode
			if (RADIO_GetState() == RADIO_STATE_SLAVE_SYNC)
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
			
		}
	}
	if (flags & TIMER_IF_CC1)
	{
		RADIO_SetState(RADIO_STATE_MASTER_TIM1_CC1);
	}
	if (flags & TIMER_IF_CC2)
	{
		RADIO_SetState(RADIO_STATE_MASTER_TIM1_CC2);
	}
	
	TIMER_IntClear(TIMER1,flags);
	
}

void TIMER3_IRQHandler()
{
	
	uint32_t flags = TIMER_IntGet(TIMER3);
	
	
	
	TIMER_IntClear(TIMER3,flags);
	
}
