#include "aloha.h"

#include "efm32_gpio.h"
#include "efm32_rtc.h"

#include "nRF24L01.h"

#include "config.h"
#include "radio.h"
#include "queue.h"
#include "tdma.h"

/* variables */
bool alohaEnabled = false;

/* functions */
void ALOHA_Enable(bool enable)
{
	
	if (enable)
	{
		
		// enable auto operation
		RADIO_EnableAutoTransmit(true);
		
		// radio to recv mode
		RADIO_SetMode(RADIO_RX);
		
	}
	
	alohaEnabled = enable;
	
}

bool ALOHA_Send()
{
	
	if (txQueue.count == 0)
		return true;
	
	// wait until suitable time to send
	uint32_t lastRecv = lastPacketReceived;
	
	// check last time received - if long enough, don't bother waiting
	
	do
	{
		lastRecv = lastPacketReceived;
		uint32_t timestamp = *(uint32_t*)(0xFE081F0);
		wait(timestamp & 0x0000000F);
	}
	while(lastRecv != lastPacketReceived);
	
	// radio to send mode
	RADIO_SetMode(RADIO_TX);
	
	// send packet
	uint8_t txPacket[33];
	
	transmitActive = true;
	RADIO_SendDequeue(&txPacket[1]);
	txPacket[0] = NRF_W_TX_PAYLOAD;
	TDMA_RadioTransfer(txPacket);
	NRF_CE_hi;
	
	// wait for send
	while (transmitActive);
	
	// radio to recv mode
	RADIO_SetMode(RADIO_RX);
	
	return true;
	
}

bool ALOHA_IsEnabled()
{
	return alohaEnabled;
}