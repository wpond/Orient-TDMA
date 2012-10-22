#include "trace.h"

#include "string.h"
#include "efm32_gpio.h"
#include "efm32_cmu.h"
#include "efm32_usart.h"

#include "led.h"

void TRACE_Init()
{
	
	USART0->CLKDIV = 256 * ((CMU_ClockFreqGet(cmuClock_USART0)) / (9600 * 16) - 1);
	
	USART0->CMD = USART_CMD_TXEN | USART_CMD_RXEN;
	
	USART0->FRAME &= ~(
    _USART_FRAME_DATABITS_MASK |
    _USART_FRAME_PARITY_MASK |
    _USART_FRAME_STOPBITS_MASK);
  USART0->FRAME |= (
    USART_FRAME_DATABITS_EIGHT |
    USART_FRAME_PARITY_NONE |
    USART_FRAME_STOPBITS_ONE
    );
	
	GPIO->P[4].DOUT |= (1 << 10);
	//GPIO->P[4].MODEH =
	//					GPIO_P_MODEH_MODE10_PUSHPULL
	//				| GPIO_P_MODEH_MODE11_INPUT;
	
	GPIO_PinModeSet(gpioPortE, 10, gpioModePushPull, 1);
	GPIO_PinModeSet(gpioPortE, 11, gpioModeInput, 1);
	
	USART0->ROUTE = USART_ROUTE_LOCATION_LOC0
          | USART_ROUTE_TXPEN | USART_ROUTE_RXPEN;
  
}

void TRACE(char* msg)
{
	
	//NVIC_DisableIRQ(GPIO_EVEN_IRQn);
	//NVIC_DisableIRQ(GPIO_ODD_IRQn);
	
	LED_On(BLUE);
	
	int todo, bytesToSend = strlen(msg);
	for (todo = 0; todo < bytesToSend; todo++) {
    while (!(USART0->STATUS & USART_STATUS_TXBL));
    USART0->TXDATA = *msg++;
  }
	
	while (!(USART0->STATUS & USART_STATUS_TXC));
	
	LED_Off(BLUE);
	
	//NVIC_EnableIRQ(GPIO_EVEN_IRQn);
	//NVIC_EnableIRQ(GPIO_ODD_IRQn);
	
}