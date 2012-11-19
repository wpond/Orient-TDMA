
#include "tasks.h"
#include "system.h"

void usb_relay_task_entrypoint()
{
	
	uint8_t packet[32];
	
	while(1)
	{
		
		// relay usb packets
		if (RADIO_Recv(packet))
		{
			USB_Transmit(packet,32);
		}
		
	}
	
}
