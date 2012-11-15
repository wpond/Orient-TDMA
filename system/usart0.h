#ifndef __USART0_H__
#define __USART0_H__

#include <stdint.h>
#include <stdbool.h>

#define MAX_USART_TRANSFERS 4

typedef enum
{
	HIGH,
	LOW
} USART0_ChipSelect;

void USART0_Init(uint8_t location);
void USART0_Transfer(uint8_t *buffer, uint16_t size, void (*cs)(USART0_ChipSelect));

#endif