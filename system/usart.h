#ifndef __USART0_H__
#define __USART0_H__

#include <stdint.h>
#include <stdbool.h>

#include "efm32_usart.h"

#include "scheduler.h"

#define MAX_USART_TRANSFERS 4
#define USART_COUNT 3
#define USART_FLAG_MASK USART0_FLAG

typedef enum
{
	HIGH,
	LOW
} USART_ChipSelect;

#define USART0_Init(location) USART_Init(USART0, 0, location)
#define USART0_Transfer(buf, size, cs_ptr) USART_Transfer(USART0, 0, buf, size, cs_ptr)

void USART_Init(USART_TypeDef *usart, uint8_t usart_idx, uint8_t location);
void USART_Transfer(USART_TypeDef *usart, uint8_t usart_idx, uint8_t *buffer, uint16_t size, void (*cs)(USART_ChipSelect));

#endif