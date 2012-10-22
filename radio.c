#include "radio.h"

#include "node_config.h"

#include "nRF24L01.h"
#include "efm32_usart.h"
#include "efm32_gpio.h"

#include "stdint.h"
#include "string.h"

#include "led.h"

/* prototypes */
uint8_t readRegister(uint8_t reg);
void writeRegister(uint8_t reg, uint8_t value);

void readBytes(uint8_t reg, uint8_t *payload, uint8_t len);
void writeBytes(uint8_t reg, uint8_t *payload, uint8_t len);

void readPayload(uint8_t *payload, uint8_t len);
void writePayload(uint8_t *payload, uint8_t len);

void readRegisterMulti(uint8_t reg, uint8_t *payload, uint8_t len);
void writeRegisterMulti(uint8_t reg, uint8_t *payload, uint8_t len);

/* variables */
volatile uint8_t bufferCount = 0;

/* functions */
void RADIO_Init()
{
	
	// usart 0 location 2
	
	// enable pins
	GPIO_PinModeSet(NRF_CE_PORT, NRF_CE_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(NRF_CSN_PORT, NRF_CSN_PIN, gpioModePushPull, 1);
	GPIO_PinModeSet(NRF_INT_PORT, NRF_INT_PIN, gpioModeInput, 0);
	GPIO_PinModeSet(NRF_RXEN_PORT, NRF_RXEN_PIN, gpioModePushPull, 0);
	
	GPIO_PinModeSet(gpioPortC, 11, gpioModePushPull, 1);
	GPIO_PinModeSet(gpioPortC, 10, gpioModeInput, 0);
	GPIO_PinModeSet(gpioPortC, 9, gpioModePushPull, 0);
	
	USART_InitSync_TypeDef usartInit = USART_INITSYNC_DEFAULT;
	
	usartInit.msbf = true;
	usartInit.clockMode = usartClockMode0;
	usartInit.baudrate = 1000000;
	USART_InitSync(NRF_USART, &usartInit);
	NRF_USART->ROUTE = (NRF_USART->ROUTE & ~_USART_ROUTE_LOCATION_MASK) | USART_ROUTE_LOCATION_LOC2;
	NRF_USART->ROUTE |= USART_ROUTE_TXPEN | USART_ROUTE_RXPEN | USART_ROUTE_CLKPEN;
	
	// configure gpio interrupt
	GPIO_IntClear(1 << 0);
	writeRegister(NRF_STATUS,0x70);
	GPIO_IntConfig(NRF_INT_PORT,NRF_INT_PIN,false,true,true);
	
	// initial config
	writeRegister(NRF_EN_AA,0x00);
	writeRegister(NRF_EN_RXADDR,0x3F);
	writeRegister(NRF_SETUP_AW,0x03);
	writeRegister(NRF_SETUP_RETR,0x00);
	writeRegister(NRF_RF_CH,NODE_CH);
	writeRegister(NRF_RF_SETUP,0x0F);
	
	uint8_t addr_array[5];
	
	addr_array[0] = 0xE7;
	addr_array[1] = 0xE7;
	addr_array[2] = 0xE7;
	addr_array[3] = 0xE7;
	addr_array[4] = 0xE7;
	
	writeRegisterMulti(NRF_TX_ADDR, addr_array, 5);
	
	writeRegisterMulti(NRF_RX_ADDR_P0, addr_array, 5);
	writeRegister(NRF_RX_PW_P0,0x20);
	
	writeRegister(NRF_DYNPD, 0x00);
	writeRegister(NRF_FEATURE, 0x00);
	
}

void RADIO_ConfigTX()
{

	writeRegister(NRF_CONFIG,0x4E);

	uint8_t nop = NRF_NOP;
	writeBytes(NRF_FLUSH_TX,&nop,1); 
	writeBytes(NRF_FLUSH_RX,&nop,1); 
	writeRegister(NRF_STATUS,0x70);
	
	bufferCount = 0;
	
}

void RADIO_ConfigRX()
{
	
	writeRegister(NRF_CONFIG,0x3F);

	uint8_t nop = NRF_NOP;
	writeBytes(NRF_FLUSH_TX,&nop,1); 
	writeBytes(NRF_FLUSH_RX,&nop,1); 
	writeRegister(NRF_STATUS,0x70);
	
	bufferCount = 0;
	
}

void RADIO_EnableRX(bool enable)
{
	
	if (enable)
	{
		NRF_RXEN_hi;
		NRF_CE_hi;
	}
	else
	{
		NRF_RXEN_lo;
		NRF_CE_lo;
	}
	
}

void RADIO_EnableTX(bool enable)
{
	
	if (enable)
	{
		NRF_CE_hi;
	}
	else
	{
		NRF_CE_lo;
	}
	
}

bool RADIO_Send(uint8_t *payload)
{
	
	uint8_t fifo_status = readRegister(NRF_FIFO_STATUS);
	
	if (fifo_status & 0x20)
	{
		return false;
	}
	
	bufferCount++;
	
	writePayload(payload, NRF_PACKET_SIZE);
	return true;
	
}

bool RADIO_Recv(uint8_t *payload)
{
	
	uint8_t fifo_status = readRegister(NRF_FIFO_STATUS);
	
	if (fifo_status & 0x01)
	{
		return false;
	}
	
	bufferCount--;
	
	readPayload(payload, NRF_PACKET_SIZE);
	return true;
	
}

uint8_t readRegister(uint8_t reg)
{
	
	volatile uint8_t value;
	
	NRF_CSN_lo;
	
	// wait for tx receive buffer to clear
	while (!(NRF_USART->STATUS & USART_STATUS_TXBL));
	
	// clear any data in buffer
	while (NRF_USART->STATUS & USART_STATUS_RXDATAV)
	{
		value = NRF_USART->RXDATA;
	}
	
	// send data
	NRF_USART->TXDATA = (reg | NRF_R_REGISTER);
	
	// wait to send
	while (!(NRF_USART->STATUS & USART_STATUS_TXBL));
	
	// read nonsense
	value = NRF_USART->RXDATA;
	
	// transmit 
	NRF_USART->TXDATA = NRF_NOP;
	
	// wait to send
	while (!(NRF_USART->STATUS & USART_STATUS_TXC));
	
	NRF_CSN_hi;
	
	// return result
	return NRF_USART->RXDATA;
	
}

void writeRegister(uint8_t reg, uint8_t value)
{
	
	volatile uint8_t nonsense;
	
	NRF_CSN_lo;
	
	// wait for tx buffer to clear
	while (!(NRF_USART->STATUS & USART_STATUS_TXBL));
	
	// clear any data in receive buffer
	while (NRF_USART->STATUS & USART_STATUS_RXDATAV)
	{
		value = NRF_USART->RXDATA;
	}
	
	// send register
	NRF_USART->TXDATA = (reg | NRF_W_REGISTER);
	
	// wait to send
	while (!(NRF_USART->STATUS & USART_STATUS_TXC));
	
	// read nonsense
	nonsense = NRF_USART->RXDATA;
	
	// send data
	NRF_USART->TXDATA = value;
	
	// wait to send
	while (!(NRF_USART->STATUS & USART_STATUS_TXC));
	
	NRF_CSN_hi;
	
}

void writeBytes(uint8_t reg, uint8_t *payload, uint8_t len)
{
	
	volatile uint8_t value;
	
	NRF_CSN_lo;
	
	// wait for tx buffer to clear
	while (!(NRF_USART->STATUS & USART_STATUS_TXBL));
	
	// clear any data in receive buffer
	while (NRF_USART->STATUS & USART_STATUS_RXDATAV)
	{
		value = NRF_USART->RXDATA;
	}
	
	// send data
	NRF_USART->TXDATA = (reg | NRF_R_REGISTER);
	
	// wait to send
	while (!(NRF_USART->STATUS & USART_STATUS_TXC));
	
	// read nonsense
	value = NRF_USART->RXDATA;
	
	uint8_t i;
	for (i = 0; i < len; i++)
	{
		
		// transmit byte
		NRF_USART->TXDATA = NRF_USART->RXDATA;
		
		// wait to send
		while (!(NRF_USART->STATUS & USART_STATUS_TXBL));
		
		// read nonsense
		value = NRF_USART->RXDATA;
		
	}
	
	NRF_CSN_hi;
	
	while (!(NRF_USART->STATUS & USART_STATUS_TXC));
	
}

void readBytes(uint8_t reg, uint8_t *payload, uint8_t len)
{
	
	volatile uint8_t value;
	
	NRF_CSN_lo;
	
	// wait for tx buffer to clear
	while (!(NRF_USART->STATUS & USART_STATUS_TXBL));
	
	// clear any data in receive buffer
	while (NRF_USART->STATUS & USART_STATUS_RXDATAV)
	{
		value = NRF_USART->RXDATA;
	}
	
	// send data
	NRF_USART->TXDATA = (reg | NRF_R_REGISTER);
	
	// wait to send
	while (!(NRF_USART->STATUS & USART_STATUS_TXC));
	
	// read nonsense
	value = NRF_USART->RXDATA;
	
	uint8_t i;
	for (i = 0; i < len; i++)
	{
		
		// transmit no operation
		NRF_USART->TXDATA = NRF_NOP;
		
		// wait to send
		while (!(NRF_USART->STATUS & USART_STATUS_TXBL));
		
		// store payload
		payload[i] = NRF_USART->RXDATA;
		
	}
	
	NRF_CSN_hi;
	
	while (!(NRF_USART->STATUS & USART_STATUS_TXC));
	
}

void readPayload(uint8_t *payload, uint8_t len)
{
	
	readBytes(NRF_R_RX_PAYLOAD, payload, len);
	
}

void writePayload(uint8_t *payload, uint8_t len)
{
	
	writeBytes(NRF_W_TX_PAYLOAD, payload, len);
	
}

void readRegisterMulti(uint8_t reg, uint8_t *payload, uint8_t len)
{
	
	readBytes((reg | NRF_R_REGISTER), payload, len);
	
}

void writeRegisterMulti(uint8_t reg, uint8_t *payload, uint8_t len)
{
	
	writeBytes((reg | NRF_R_REGISTER), payload, len);
	
}

void RADIO_Interrupt()
{
	
	if (GPIO_IntGet() & NRF_INT_PIN)
	{
		
		// handle interrupt
		uint8_t status = readRegister(NRF_STATUS);
		
		// tx
		if (status & 0x20)
		{
			
			bufferCount--;
			
			// if nothing left to sendk
			if (bufferCount == 0)
			{
				NRF_CE_lo;
				
				#ifdef BS
				RADIO_QueueTDMAPulse();
				#endif
				
			}
			
		}
		
		// rx
		if (status & 0x40)
		{
			
			bufferCount++;
			
		}
		
		writeRegister(NRF_STATUS,0x70);
		GPIO_IntClear(NRF_INT_PIN);
		
	}
	
}

void RADIO_QueueTDMAPulse()
{
	
	uint8_t pulse[32];
	
	pulse[0] = 0xFF;
	
	uint8_t i;
	for (i = 1; i < 32; i++)
	{
		pulse[i] = 0;
	}
	
	RADIO_Send(pulse);
	
}

uint8_t RADIO_GetBufferFill()
{
	
		return bufferCount;
		
}
