#include "radio_task.h"

#include "efm32_int.h"
#include "efm32_usart.h"

#include <string.h>

#include "tasks.h"
#include "system.h"

#include "nRF24L01.h"
#include "led.h"

/* variables */

/* prototypes */
void radio_cs(USART0_ChipSelect set);
uint8_t radio_readRegister(uint8_t reg);
void radio_writeRegister(uint8_t reg, uint8_t value);

/* functions */
void GPIO_EVEN_IRQHandler()
{
	GPIO_IntClear((1 << NRF_INT_PIN));
	return;
	
	if (GPIO_IntGet() & (1 << NRF_INT_PIN))
	{
		
		// handle interrupt
		uint8_t status = radio_readRegister(NRF_STATUS);
		
		// tx
		if (status & 0x20)
		{
			
			LED_On(BLUE);
			
		}
		
		// rx
		if (status & 0x40)
		{
			
			
			
		}
		
		radio_writeRegister(NRF_STATUS,0x70);
		//GPIO_IntClear((1 << NRF_INT_PIN));
		
	}
	
}

void radio_task_entrypoint()
{
	
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
	radio_writeRegister(NRF_EN_RXADDR,0x01);
	radio_writeRegister(NRF_SETUP_AW,0x03);
	radio_writeRegister(NRF_SETUP_RETR,0x00);
	radio_writeRegister(NRF_RF_CH,40);
	radio_writeRegister(NRF_RF_SETUP,0x0F);
	
	radio_writeRegister(NRF_RX_PW_P0,32);
	
	uint8_t addr[6];
	memset(addr,0xE7,6);
	addr[0] = 0x00 | (NRF_W_REGISTER | NRF_TX_ADDR);
	USART0_Transfer(addr,6,radio_cs);
	
	addr[0] = 0x00 | (NRF_W_REGISTER | NRF_RX_ADDR_P0);
	USART0_Transfer(addr,6,radio_cs);
	
	radio_writeRegister(NRF_DYNPD, 0x00);
	radio_writeRegister(NRF_FEATURE, 0x00);
	
	radio_writeRegister(NRF_CONFIG,0x0E);
	
	uint8_t p[33];
	
	while ((~radio_readRegister(NRF_FIFO_STATUS)) & 0x20)
	{
		
		p[0] = NRF_W_TX_PAYLOAD;
		memset(&p[1],NRF_NOP,32);
		
		USART0_Transfer(p,33,radio_cs);
		
	}
	
	NRF_CE_hi;
	
	while ((~radio_readRegister(NRF_FIFO_STATUS)) & 0x10);
	
	NRF_CE_lo;
	
	LED_On(GREEN);
	LED_On(RED);
	LED_On(BLUE);
	
	while(1);
	
}

void radio_cs(USART0_ChipSelect set)
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
