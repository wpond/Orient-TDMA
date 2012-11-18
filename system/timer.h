#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "efm32_timer.h"

#define MAX_TIMER_CALLBACKS 8

typedef struct
{
	
	TIMER_TypeDef *timer;
	uint32_t flags;
	void (*cb)(void);
	
} timer_cb_table_t;

void TIMER_InitCallbacks();
bool TIMER_RegisterCallback(timer_cb_table_t *entry);
void TIMER_ClearCallback(timer_cb_table_t entry);

#endif