#ifndef __RADIO_H__

	#define __RADIO_H__
	
	#include "efm32_gpio.h"
	#include "efm32_usart.h"
	
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
	
	#define RADIO_SEND_QUEUE_SIZE 128
	
	#define RADIO_DATA_SIZE 1024
	
	#define UNRESERVED_SLOT_COUNT 5
	#define UNRESERVED_SLOT_OFFSET 5

	typedef enum
	{
		RADIO_OFF = 0x0C,
		RADIO_TX = 0x0E,
		RADIO_RX = 0x0F
	}
	RADIO_Mode;
	
	typedef struct
	{
		bool master;
		uint8_t channel;
		uint8_t slot;
		uint8_t slotCount;
		uint32_t guardPeriod;
		uint32_t transmitPeriod;
		uint32_t protectionPeriod;
	}
	RADIO_TDMAConfig;
	
	extern uint16_t dataRate;
	
	void RADIO_Init();
	void RADIO_SetMode(RADIO_Mode mode);
	RADIO_Mode RADIO_GetMode();
	bool RADIO_Send(const uint8_t packet[32]);
	bool RADIO_Recv(uint8_t packet[32]);
	void RADIO_IRQHandler();
	void RADIO_Main();
	void RADIO_ConfigTDMA(RADIO_TDMAConfig *_config);
	void RADIO_EnableTDMA();
	void RADIO_DisableTDMA();
	bool RADIO_SendData(const uint8_t *data, uint16_t len);
	void RADIO_BasestationReset();
	
#endif