
#include "system.h"
#include "tasks.h"

#include "radio_init_task.h"
#include "led.h"

void node_radio_task_entrypoint()
{
	
	RADIO_Enable(OFF);
	RADIO_SetMode(RX);
	RADIO_Enable(RX);
	
	while(1)
	{
		uint8_t payload[32];
		
		if (RADIO_Recv(payload))
		{
			LED_Toggle(GREEN);
		}
		
	}
	
}
