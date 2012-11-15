#ifndef __LED_H__
#define __LED_H__

#include "stdint.h"

typedef struct
{
	uint8_t port;
	uint8_t pin;
} LED_Config;

typedef enum
{
	RED = 0,
	BLUE = 1,
	GREEN = 2,
} LED;

void LED_Init();
void LED_On(LED led);
void LED_Off(LED led);
void LED_Toggle(LED led);

#endif