/*

Main file for SLIP D embedded software

*/

/* includes */
#include "efm32.h"

#include "efm32_chip.h"
#include "efm32_rtc.h"
#include "efm32_gpio.h"
#include "efm32_cmu.h"
#include "efm32_timer.h"
#include "efm32_int.h"

#include "stdint.h"
#include "stdbool.h"

#include "led.h"
#include "trace.h"
#include "radio.h"
#include "timer.h"

#include "node_config.h"

/* variables */

/* prototypes */
void InitClocks();
void HandleInterrupt();
void startupLEDs();
void wait(uint32_t ms);

/* functions */
void TIMER0_IRQHandler(void)
{ 
  
  TIMER_IntClear(TIMER0, TIMER_IF_CC2);
  
  
  
}

void TIMER1_IRQHandler(void)
{ 
  
  LED_Toggle(BLUE);
  
  TIMER_IntClear(TIMER1, TIMER_IF_CC0);
  TIMER_IntClear(TIMER1, TIMER_IF_CC2);
  
  TIMER_IntClear(TIMER1, TIMER_IF_OF);
  
}

// messy interrupt handler
void GPIO_EVEN_IRQHandler(void) 
{
	HandleInterrupt();
}
void GPIO_ODD_IRQHandler(void)
{
	HandleInterrupt();
}

void HandleInterrupt()
{
	
	RADIO_Interrupt();
	
}

void wait(uint32_t ms)
{
	
	uint32_t time, 
		clockFreq = CMU_ClockFreqGet(cmuClock_RTC);
	
	while (ms > 0)
	{
		
		time = RTC_CounterGet();
		
		if (16777215 - time < ((double)ms / 1000.0) * clockFreq)
		{
			ms -= (uint32_t)(1000.0 * ((16777215 - time) / (double)clockFreq));
			while (RTC_CounterGet() > time);
		}
		else
		{
			while (RTC_CounterGet() < time + ((double)ms / 1000.0) * clockFreq);
			break;
		}
		
	}
	
}

void startupLEDs()
{
	
	LED_Off(RED);
	LED_Off(BLUE);
	LED_Off(GREEN);
	
	wait(1000);
	
	LED_On(RED);
	LED_On(BLUE);
	LED_On(GREEN);
	
	wait(1000);
	
	LED_Off(RED);
	LED_Off(BLUE);
	LED_Off(GREEN);
	
	wait(1000);
	
}

void InitClocks()
{
	/* Starting LFXO and waiting until it is stable */
	CMU_OscillatorEnable(cmuOsc_LFXO, true, true);

	// starting HFXO, wait till stable
	CMU_OscillatorEnable(cmuOsc_HFXO, true, true);

	// route HFXO to CPU
	CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFXO);

	/* Routing the LFXO clock to the RTC */
	CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO);
	CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_LFXO);

	// disabling the RCs
	CMU_ClockEnable(cmuSelect_HFRCO, false);
	CMU_ClockEnable(cmuSelect_LFRCO, false);

	/* Enabling clock to the interface of the low energy modules */
	CMU_ClockEnable(cmuClock_CORE, true);
	CMU_ClockEnable(cmuClock_CORELE, true);

	// enable clock to hf perfs
	CMU_ClockEnable(cmuClock_HFPER, true);

	// enable clock to GPIO
	CMU_ClockEnable(cmuClock_GPIO, true);

	// enable clock to RTC
	CMU_ClockEnable(cmuClock_RTC, true);
	RTC_Enable(true);
	
	// enable radio usart
	CMU_ClockEnable(cmuClock_USART0, true);

	// enable timers 
	CMU_ClockEnable(cmuClock_TIMER0, true);
	CMU_ClockEnable(cmuClock_TIMER1, true);

}

int main()
{
	
	// Chip errata
	CHIP_Init();
	
	// ensure core frequency has been updated
	SystemCoreClockUpdate();
	
	// start clocks
	InitClocks();
	
	// init LEDs
	LED_Init();
	
	// show startup LEDs
	startupLEDs();
	
	// enable gpio interrupts
	NVIC_ClearPendingIRQ(GPIO_EVEN_IRQn);
	NVIC_ClearPendingIRQ(GPIO_ODD_IRQn);
	
	NVIC_EnableIRQ(GPIO_EVEN_IRQn);
	NVIC_EnableIRQ(GPIO_ODD_IRQn);
	
	// start radio
	RADIO_Init();
	
	#ifdef BASESTATION
	
	RADIO_QueueTDMAPulse();
	
	//TDMATIMER_ConfigBS();
	
	TDMATIMER_Init();
	
	LED_On(GREEN);
	
	#elif defined(NODE)
	
	TDMATIMER_ConfigNode();
	
	TDMATIMER_Init();
	
	INT_Enable();
	
	while (1)
	{
		
		uint8_t payload[32];
		
		while (RADIO_GetBufferFill() > 0)
		{
			
			LED_On(BLUE);
			
			RADIO_Recv(payload);
			
			if (payload[0] == 0xFF)
			{
				LED_Toggle(GREEN);
			}
			else
			{
				LED_Toggle(RED);
			}
			
		}
		
	}
	
	#endif
	
	
	while (1);
	
}