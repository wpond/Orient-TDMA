
#include "system.h"
#include "tasks.h"

#include "radio_init_task.h"
#include "led.h"
#include "packets.h"

void node_radio_task_entrypoint()
{
	
	RADIO_Enable(OFF);
	RADIO_SetMode(RX);
	RADIO_Enable(RX);
	
	while(1)
	{
		uint8_t packet[32];
		
		if (RADIO_Recv(packet))
		{
			
			if (packet[1] == 0xFF &&
				packet[2] == 0x00)
				LED_Toggle(RED);
		}
		
	}
	
}
