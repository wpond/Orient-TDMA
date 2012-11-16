#include "timer.h"

#include "efm32_int.h"

/* variables */
timer_cb_table_t timer_cb_table[MAX_TIMER_CALLBACKS];

/* prototyes */
uint32_t TIMER_Callbacks();

/* functions */
uint32_t TIMER_Callbacks(TIMER_TypeDef *timer, uint32_t flags)
{
	
	uint32_t handled_flags = 0;
	int i;
	for (i = 0; i < MAX_TIMER_CALLBACKS; i++)
	{
		
		if (timer_cb_table[i].timer == timer && timer_cb_table[i].flags & flags)
		{
			
			timer_cb_table[i].cb();
			handled_flags &= timer_cb_table[i].flags;
			
		}
		
	}
	
	return handled_flags;
	
}

void TIMER_InitCallbacks()
{
	
	int i;
	for (i = 0; i < MAX_TIMER_CALLBACKS; i++)
	{
		timer_cb_table[i].cb = NULL;
		timer_cb_table[i].timer = NULL;
		timer_cb_table[i].flags = 0;
	}
	
}

bool TIMER_RegisterCallback(timer_cb_table_t entry)
{
	
	int i;
	INT_Disable();
	for (i = 0; i < MAX_TIMER_CALLBACKS; i++)
	{
		
		if (timer_cb_table[i].flags == 0)
		{
			
			memcpy(&timer_cb_table[i], &entry, sizeof(timer_cb_table_t));
			
			INT_Enable();
			return true;
			
		}
		
	}
	INT_Enable();
	return false;
	
}

void TIMER_ClearCallback(timer_cb_table_t entry)
{
	
	int i;
	INT_Disable();
	for (i = 0; i < MAX_TIMER_CALLBACKS; i++)
	{
		
		if (memcmp(&timer_cb_table[i], &entry, sizeof(timer_cb_table_t)) == 0)
		{
			memset(&timer_cb_table[i], 0, sizeof(timer_cb_table_t));
		}
		
	}
	INT_Enable();
	
}
