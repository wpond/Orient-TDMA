#ifndef __RADIO_TASK_H__
#define __RADIO_TASK_H__

#include <stdint.h>
#include <stdbool.h>

#include "efm32_gpio.h"

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

#define RADIO_BUFFER_SIZE 		16

/*
#define TDMA_SLOT_COUNT				255
#define TDMA_SLOT_WIDTH 			0.003
#define TDMA_GUARD_PERIOD 		0.0005
*/
#define TDMA_SLOT_COUNT				2
#define TDMA_SLOT_WIDTH 			0.4
#define TDMA_GUARD_PERIOD 		0.05

typedef enum 
{
	OFF,
	TX,
	RX
} RADIO_Mode;

bool RADIO_Send(uint8_t payload[32]);
bool RADIO_Recv(uint8_t payload[32]);
void RADIO_SetMode(RADIO_Mode rm);
void RADIO_Enable(RADIO_Mode rm);
void RADIO_TxBufferFill();
void RADIO_SetAutoRefil(bool auto_refil);

#endif