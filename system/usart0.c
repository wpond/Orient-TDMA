#include "usart0.h"

#include <stdbool.h>

#include "efm32_int.h"
#include "efm32_usart.h"

#include "system.h"
#include "scheduler.h"

#include "led.h"

/* variables */
typedef struct
{
	
	bool *complete, active;
	uint8_t *buffer;
	uint16_t txPosition, rxPosition, size;
	void (*cs) (USART0_ChipSelect);
	
} usart_transfer_t;
usart_transfer_t usart_transfers[MAX_USART_TRANSFERS];
uint8_t usart_transfers_transfer_position = 0,
	usart_transfers_write_position = 0;

/* prototypes */
void usart0_enableInterrupts();
void usart0_disableInterrupts();

/* functions */
void USART0_Init(uint8_t location)
{
	
	uint32_t usart_location;
	switch (location)
	{
	case 0:
		usart_location = USART_ROUTE_LOCATION_LOC0;
		break;
	case 1:
		usart_location = USART_ROUTE_LOCATION_LOC1;
		break;
	case 2:
		usart_location = USART_ROUTE_LOCATION_LOC2;
		break;
	}
	
	USART_InitSync_TypeDef usartInit = USART_INITSYNC_DEFAULT;
	
	usartInit.msbf = true;
	usartInit.clockMode = usartClockMode0;
	usartInit.baudrate = 1000000;
	USART_InitSync(USART0, &usartInit);
	USART0->ROUTE = (USART0->ROUTE & ~_USART_ROUTE_LOCATION_MASK) | usart_location;
	USART0->ROUTE |= USART_ROUTE_TXPEN | USART_ROUTE_RXPEN | USART_ROUTE_CLKPEN;
	
}

void usart0_enableInterrupts()
{
	USART_IntEnable(USART0,USART_IF_TXBL);
	USART_IntEnable(USART0,USART_IF_RXDATAV);
}

void usart0_disableInterrupts()
{
	USART_IntDisable(USART0,USART_IF_TXBL);
	//USART_IntDisable(USART0,USART_IF_RXDATAV);
}

void USART0_TX_IRQHandler()
{
	
	if (!(USART0->STATUS & USART_STATUS_TXBL))
		return;
	
	if (usart_transfers_transfer_position == usart_transfers_write_position)
	{
		usart0_disableInterrupts();
		return;
	}
	
	usart_transfer_t *transfer = &usart_transfers[usart_transfers_transfer_position];
	
	if (transfer->txPosition == 0)
	{
		transfer->cs(LOW);
		transfer->active = true;
	}
	else if (transfer->txPosition >= transfer->size)
	{
		USART_IntDisable(USART0,USART_IF_TXBL);
		return;
	}
	
	USART0->TXDATA = transfer->buffer[transfer->txPosition];
	transfer->txPosition++;
	
}

void USART0_RX_IRQHandler()
{
	
	if (!(USART0->STATUS & USART_STATUS_RXDATAV))
	{
		return;
	}
	
	if (usart_transfers_transfer_position == usart_transfers_write_position)
	{
		volatile uint8_t scratch;
		scratch = USART0->RXDATA;
		usart0_disableInterrupts();
		return;
	}
	
	usart_transfer_t *transfer = &usart_transfers[usart_transfers_transfer_position];
	
	if (transfer->active == false)
	{
		volatile uint8_t scratch;
		scratch = USART0->RXDATA;
		return;
	}
	
	transfer->buffer[transfer->rxPosition] = USART0->RXDATA;
	transfer->rxPosition++;
	
	if (transfer->rxPosition == transfer->size)
	{
		transfer->cs(HIGH);
		*(transfer->complete) = true;
		transfer->active = false;
		usart_transfers_transfer_position = (usart_transfers_transfer_position + 1) % MAX_USART_TRANSFERS;
		USART_IntEnable(USART0,USART_IF_TXBL);
		return;
	}
	
}

void USART0_Transfer(uint8_t *buffer, uint16_t size, void (*cs)(USART0_ChipSelect))
{
	
	// create transfer
	bool complete = false;
	
	usart_transfer_t transfer;
	transfer.complete = &complete;
	transfer.active = false;
	transfer.buffer = buffer;
	transfer.txPosition = 0;
	transfer.rxPosition = 0;
	transfer.size = size;
	transfer.cs = cs;
	
	// find next space in queue
	// 	while no space, sleep task
	do
	{
		
		INT_Disable();
		if ((usart_transfers_write_position + 1) % MAX_USART_TRANSFERS != usart_transfers_transfer_position)
		{
			usart_transfers[usart_transfers_write_position] = transfer;
			usart_transfers_write_position = (usart_transfers_write_position + 1) % MAX_USART_TRANSFERS;
			break;
			
		}
		
		INT_Enable();
		SCHEDULER_Wait(USART0_FLAG);
		
	}
	while (1);
	
	usart0_enableInterrupts();
	INT_Enable();
	
	// wait for transfer to complete
	while (!complete);
	SCHEDULER_Release(USART0_FLAG);
	
}
