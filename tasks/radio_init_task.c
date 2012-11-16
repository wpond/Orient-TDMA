#include "radio_init_task.h"

#include "efm32_int.h"
#include "efm32_usart.h"

#include <string.h>

#include "config.h"
#include "tasks.h"
#include "system.h"

#include "nRF24L01.h"
#include "led.h"

/* variables */
typedef struct
{
	
	uint8_t read_position,
		write_position;
	uint8_t buffer[RADIO_BUFFER_SIZE][32];
	
}	RADIO_Buffer;

static RADIO_Buffer txBuffer, rxBuffer;
static bool auto_refil = true;

/* prototypes */
void radio_cs(USART_ChipSelect set);
uint8_t radio_readRegister(uint8_t reg);
void radio_writeRegister(uint8_t reg, uint8_t value);
void radio_interrupt_rt();
void radio_initBuffer(RADIO_Buffer *buf);
bool radio_writeBuffer(RADIO_Buffer *buf, uint8_t payload[32]);
bool radio_readBuffer(RADIO_Buffer *buf, uint8_t payload[32]);

/* functions */
void GPIO_EVEN_IRQHandler()
{
	
	if (GPIO_IntGet() & (1 << NRF_INT_PIN))
	{
		
		SCHEDULER_RunRTTask(&radio_interrupt_rt);
		
		GPIO_IntClear((1 << NRF_INT_PIN));
		
	}
	
}

void radio_interrupt_rt()
{
	
	uint8_t status = radio_readRegister(NRF_STATUS);
	
	uint8_t clr = 0x00;
	
	// max rt
	if (status & 0x10)
	{
		clr |= 0x10;
	}
	
	// tx
	if (status & 0x20)
	{
		
		if (auto_refil)
		{
			RADIO_TxBufferFill();
		}
		
		clr |= 0x20;
	}
	
	// rx
	if (status & 0x40)
	{
		
		// store all received packets
		uint8_t payload[33];
		while (!(radio_readRegister(NRF_FIFO_STATUS) & 0x01))
		{
			payload[0] = NRF_R_RX_PAYLOAD;
			USART0_Transfer(payload,33,radio_cs);
			radio_writeBuffer(&rxBuffer, &payload[1]);
		}
		
		clr |= 0x40;
	}
	
	radio_writeRegister(NRF_STATUS,clr);
	
}

void radio_init_task_entrypoint()
{
	
	radio_initBuffer(&txBuffer);
	radio_initBuffer(&rxBuffer);
	
	GPIO_PinModeSet(NRF_CE_PORT, NRF_CE_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(NRF_CSN_PORT, NRF_CSN_PIN, gpioModePushPull, 1);
	GPIO_PinModeSet(NRF_RXEN_PORT, NRF_RXEN_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(NRF_INT_PORT, NRF_INT_PIN, gpioModeInput, 0);
	
	GPIO_PinModeSet(gpioPortC, 11, gpioModePushPull, 1);
	GPIO_PinModeSet(gpioPortC, 10, gpioModeInput, 0);
	GPIO_PinModeSet(gpioPortC, 9, gpioModePushPull, 0);
	
	USART0_Init(2);
	
	// configure gpio interrupt
	radio_writeRegister(NRF_STATUS,0x70);
	GPIO_IntClear(1 << NRF_INT_PIN);
	GPIO_IntConfig(NRF_INT_PORT,NRF_INT_PIN,false,true,true);
	
	radio_writeRegister(NRF_EN_AA,0x00);
	radio_writeRegister(NRF_EN_RXADDR,0x3F);
	radio_writeRegister(NRF_SETUP_AW,0x03);
	radio_writeRegister(NRF_SETUP_RETR,0x00);
	radio_writeRegister(NRF_RF_CH,2);
	radio_writeRegister(NRF_RF_SETUP,0x0F);
	
	radio_writeRegister(NRF_RX_PW_P0,32);
	
	uint8_t addr[6];
	memset(addr,0xE7,6);
	addr[0] = 0x00 | (NRF_W_REGISTER | NRF_TX_ADDR);
	USART0_Transfer(addr,6,radio_cs);
	
	memset(addr,0xE7,6);
	addr[0] = 0x00 | (NRF_W_REGISTER | NRF_RX_ADDR_P0);
	USART0_Transfer(addr,6,radio_cs);
	
	radio_writeRegister(NRF_DYNPD, 0x00);
	radio_writeRegister(NRF_FEATURE, 0x00);
	
	RADIO_Enable(OFF);
	RADIO_SetMode(OFF);
	
	#ifdef BASESTATION
		
		SCHEDULER_TaskInit(&basestation_radio_task, basestation_radio_task_entrypoint);
		
	#else
	
		SCHEDULER_TaskInit(&node_radio_task, node_radio_task_entrypoint);
	
	#endif
	
}

void radio_cs(USART_ChipSelect set)
{
	
	switch (set)
	{
	case HIGH:
		NRF_CSN_hi;
		break;
	case LOW:
		NRF_CSN_lo;
		break;
	}
	
}

void RADIO_SetMode(RADIO_Mode rm)
{
	
	uint8_t cmd = NRF_FLUSH_RX;
	USART0_Transfer(&cmd,1,radio_cs);
	cmd = NRF_FLUSH_TX;
	USART0_Transfer(&cmd,1,radio_cs);
	
	switch (rm)
	{
		default:
		case OFF:
			radio_writeRegister(NRF_CONFIG, 0x0C);
			break;
		case TX:
			radio_writeRegister(NRF_CONFIG, 0x0E);
			break;
		case RX:
			radio_writeRegister(NRF_CONFIG, 0x0F);
			break;
	}
}

void RADIO_Enable(RADIO_Mode rm)
{
	switch (rm)
	{
		default:
		case OFF:
			NRF_RXEN_lo;
			NRF_CE_lo;
			break;
		case TX:
			NRF_RXEN_lo;
			NRF_CE_hi;
			break;
		case RX:
			NRF_CE_hi;
			NRF_RXEN_hi;
			break;
	}
}

uint8_t radio_readRegister(uint8_t reg)
{
	uint8_t transfer[2];
	transfer[0] = (NRF_R_REGISTER | reg);
	transfer[1] = NRF_NOP;
	USART0_Transfer(transfer,2,radio_cs);
	return transfer[1];
}

void radio_writeRegister(uint8_t reg, uint8_t value)
{
	uint8_t transfer[2];
	transfer[0] = (NRF_W_REGISTER | reg);
	transfer[1] = value;
	USART0_Transfer(transfer,2,radio_cs);
}

bool RADIO_Send(uint8_t payload[32])
{
	
	return radio_writeBuffer(&txBuffer, payload);
	
}

bool RADIO_Recv(uint8_t payload[32])
{
	
	return radio_readBuffer(&rxBuffer, payload);
	
}

void RADIO_SetAutoRefil(bool _auto_refil)
{
	auto_refil = _auto_refil;
}

void RADIO_TxBufferFill()
{
	
	uint8_t payload[33];
	
	while ((!(radio_readRegister(NRF_FIFO_STATUS) & 0x20)) && (radio_readBuffer(&txBuffer,&payload[1])))
	{
		
		payload[0] = NRF_W_TX_PAYLOAD;
		USART0_Transfer(payload,33,radio_cs);
		
	}
	
}

void radio_initBuffer(RADIO_Buffer *buf)
{
	
	buf->read_position = 0;
	buf->write_position = 0;
	
}

bool radio_writeBuffer(RADIO_Buffer *buf, uint8_t payload[32])
{
	
	if ((buf->write_position + 1) % RADIO_BUFFER_SIZE == buf->read_position)
		return false;
	
	buf->write_position = (buf->write_position + 1) % RADIO_BUFFER_SIZE;
	memcpy(buf->buffer[buf->write_position],payload,32);
	
	return true;
	
}

bool radio_readBuffer(RADIO_Buffer *buf, uint8_t payload[32])
{
	
	if (buf->write_position == buf->read_position)
		return false;
	
	memcpy(payload, buf->buffer[buf->read_position], 32);
	buf->read_position = (buf->read_position + 1) % RADIO_BUFFER_SIZE;
	
	return true;
	
}
