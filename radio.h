#ifndef __RADIO_H__

	#define __RADIO_H__
	
	#include <stdbool.h>
	#include <stdint.h>

	#define NRF_CE_PORT 3
	#define NRF_CE_PIN 6
	#define NRF_INT_PORT 5
	#define NRF_INT_PIN 8
	#define NRF_CSN_PORT 5
	#define NRF_CSN_PIN 5
	#define NRF_RXEN_PORT 4
	#define NRF_RXEN_PIN 12

	#define NRF_CE_lo GPIO_PinOutClear(NRF_CE_PORT, NRF_CE_PIN)
	#define NRF_CE_hi GPIO_PinOutSet(NRF_CE_PORT, NRF_CE_PIN)

	#define NRF_CSN_lo GPIO_PinOutClear(NRF_CSN_PORT, NRF_CSN_PIN)
	#define NRF_CSN_hi GPIO_PinOutSet(NRF_CSN_PORT, NRF_CSN_PIN)

	#define NRF_RXEN_lo GPIO_PinOutClear(NRF_RXEN_PORT, NRF_RXEN_PIN)
	#define NRF_RXEN_hi GPIO_PinOutSet(NRF_RXEN_PORT, NRF_RXEN_PIN)
	
	#define RADIO_TRANSFER_QUEUE_SIZE 8
	#define RADIO_SEND_QUEUE_SIZE 16
	#define RADIO_RECV_QUEUE_SIZE 16
	
	typedef enum
	{
		RADIO_OFF,
		RADIO_TX,
		RADIO_RX
	}
	RADIO_Mode;
	
	void RADIO_Init();
	bool RADIO_Send(uint8_t packet[32]);
	bool RADIO_Recv(uint8_t packet[32]);
	void RADIO_SetMode(RADIO_Mode mode);
	void RADIO_IRQHandler();
	
#endif