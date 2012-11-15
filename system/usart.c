#include "usart.h"

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
	void (*cs) (USART_ChipSelect);
	
} usart_transfer_t;
usart_transfer_t usart_transfers[USART_COUNT][MAX_USART_TRANSFERS];
uint8_t usart_transfers_transfer_position[USART_COUNT],
	usart_transfers_write_position[USART_COUNT];

/* prototypes */
void usart_enableInterrupts(USART_TypeDef *usart);
void usart_disableInterrupts(USART_TypeDef *usart);
void USART_TX(USART_TypeDef *usart, uint8_t usart_idx);
void USART_RX(USART_TypeDef *usart, uint8_t usart_idx);

/* functions */
void USART_Init(USART_TypeDef *usart, uint8_t usart_idx, uint8_t location)
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
	USART_InitSync(usart, &usartInit);
	usart->ROUTE = (usart->ROUTE & ~_USART_ROUTE_LOCATION_MASK) | usart_location;
	usart->ROUTE |= USART_ROUTE_TXPEN | USART_ROUTE_RXPEN | USART_ROUTE_CLKPEN;
	
	usart_transfers_transfer_position[usart_idx] = 0;
	usart_transfers_write_position[usart_idx] = 0;
	
}

void usart_enableInterrupts(USART_TypeDef *usart)
{
	USART_IntEnable(usart,USART_IF_TXBL);
	USART_IntEnable(usart,USART_IF_RXDATAV);
}

void usart_disableInterrupts(USART_TypeDef *usart)
{
	USART_IntDisable(usart,USART_IF_TXBL);
}

void USART0_TX_IRQHandler()
{
	
	USART_TX(USART0,0);
	
}

void USART0_RX_IRQHandler()
{
	
	USART_RX(USART0,0);
	
}

void USART_RX(USART_TypeDef *usart, uint8_t usart_idx)
{
	
	if (!(usart->STATUS & USART_STATUS_RXDATAV))
	{
		return;
	}
	
	if (usart_transfers_transfer_position[usart_idx] == usart_transfers_write_position[usart_idx])
	{
		volatile uint8_t scratch;
		scratch = usart->RXDATA;
		usart_disableInterrupts(usart);
		return;
	}
	
	usart_transfer_t *transfer = &usart_transfers[usart_idx][usart_transfers_transfer_position[usart_idx]];
	
	if (transfer->active == false)
	{
		volatile uint8_t scratch;
		scratch = usart->RXDATA;
		return;
	}
	
	transfer->buffer[transfer->rxPosition] = usart->RXDATA;
	transfer->rxPosition++;
	
	if (transfer->rxPosition == transfer->size)
	{
		transfer->cs(HIGH);
		*(transfer->complete) = true;
		transfer->active = false;
		usart_transfers_transfer_position[usart_idx] = (usart_transfers_transfer_position[usart_idx] + 1) % MAX_USART_TRANSFERS;
		USART_IntEnable(usart,USART_IF_TXBL);
		return;
	}
	
}

void USART_TX(USART_TypeDef *usart, uint8_t usart_idx)
{
	
	if (!(usart->STATUS & USART_STATUS_TXBL))
		return;
	
	if (usart_transfers_transfer_position[usart_idx] == usart_transfers_write_position[usart_idx])
	{
		usart_disableInterrupts(usart);
		return;
	}
	
	usart_transfer_t *transfer = &usart_transfers[usart_idx][usart_transfers_transfer_position[usart_idx]];
	
	if (transfer->txPosition == 0)
	{
		transfer->cs(LOW);
		transfer->active = true;
	}
	else if (transfer->txPosition >= transfer->size)
	{
		USART_IntDisable(usart,USART_IF_TXBL);
		return;
	}
	
	usart->TXDATA = transfer->buffer[transfer->txPosition];
	transfer->txPosition++;
	
}

void USART_Transfer(USART_TypeDef *usart, uint8_t usart_idx, uint8_t *buffer, uint16_t size, void (*cs)(USART_ChipSelect))
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
		if ((usart_transfers_write_position[usart_idx] + 1) % MAX_USART_TRANSFERS != usart_transfers_transfer_position[usart_idx])
		{
			usart_transfers[usart_idx][usart_transfers_write_position[usart_idx]] = transfer;
			usart_transfers_write_position[usart_idx] = (usart_transfers_write_position[usart_idx] + 1) % MAX_USART_TRANSFERS;
			break;
			
		}
		
		INT_Enable();
		SCHEDULER_Wait((USART_FLAG_MASK << usart_idx));
		
	}
	while (1);
	
	usart_enableInterrupts(usart);
	INT_Enable();
	
	// wait for transfer to complete
	while (!complete);
	SCHEDULER_Release((USART_FLAG_MASK << usart_idx));
	
}
