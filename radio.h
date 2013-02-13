#ifndef __RADIO_H__
	
	#define __RADIO_H__
	
	#include <stdint.h>
	#include <stdbool.h>
	
	#include "queue.h"
	#include "packets.h"
	
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
	
	#define RADIO_USART USART0
	#define RADIO_USART_LOCATION USART_ROUTE_LOCATION_LOC2
	
	#define RADIO_SEND_QUEUE_SIZE 32
	#define RADIO_RECV_QUEUE_SIZE 40
	
	#define BROADCAST_ID 0xFF
	
	typedef enum
	{
		RADIO_OFF = 0x0C,
		RADIO_TX = 0x0E,
		RADIO_RX = 0x0F
	}
	RADIO_Mode;
	
	queue_t rxQueue,
		txQueue;
	bool transferActive,
		transmitActive;
	uint32_t lastPacketReceived;
	
	void RADIO_Init();
	void RADIO_SetMode(RADIO_Mode mode);
	bool RADIO_Send(uint8_t packet[32]);
	bool RADIO_Recv(uint8_t packet[32]);
	bool RADIO_SendDequeue(uint8_t packet[32]);
	void RADIO_SetChannel(uint8_t channel);
	
	void RADIO_IRQHandler();
	void RADIO_EnableAutoTransmit(bool enable);
	
	bool RADIO_HandleIncomingPacket(PACKET_Raw *packet);
	
#endif