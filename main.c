/* includes */
#include "efm32.h"

#include "efm32_chip.h"
#include "efm32_dma.h"
#include "efm32_rtc.h"
#include "efm32_gpio.h"
#include "efm32_emu.h"
#include "efm32_cmu.h"
#include "efm32_timer.h"
#include "efm32_int.h"

#include "stdint.h"
#include "stdbool.h"

#include "led.h"
#include "radio.h"
#include "usb.h"

/* variables */

/* prototypes */
void InitClocks();
void HandleInterrupt();
void StartupLEDs();
void EnableInterrupts();
void wait(uint32_t ms);

void mainTaskEntrypoint();

/* interrupts */
void GPIO_EVEN_IRQHandler()
{
	
	while (GPIO_IntGet() & (1 << NRF_INT_PIN))
	{
		
		RADIO_IRQHandler();
		GPIO_IntClear((1 << NRF_INT_PIN));
		
	}
	
}

/* functions */
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

void StartupLEDs()
{
	
	LED_Off(RED);
	LED_Off(BLUE);
	LED_Off(GREEN);
	
	wait(500);
	
	LED_On(RED);
	LED_On(BLUE);
	LED_On(GREEN);
	
	wait(500);
	
	LED_Off(RED);
	LED_Off(BLUE);
	LED_Off(GREEN);
	
	wait(500);
	
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
	RTC_Reset();
	RTC_Enable(true);
	
	// enable radio usart
	CMU_ClockEnable(cmuClock_USART0, true);
	
	// enable dma clock
	CMU_ClockEnable(cmuClock_DMA, true);
	DMA_Reset();
	
	// enable timers 
	CMU_ClockEnable(cmuClock_TIMER0, true);
	CMU_ClockEnable(cmuClock_TIMER1, true);
	CMU_ClockEnable(cmuClock_TIMER3, true);
	TIMER_Reset(TIMER0);
	TIMER_Reset(TIMER1);
	TIMER_Reset(TIMER3);
	
}

void EnableInterrupts()
{
	
	NVIC_EnableIRQ(GPIO_EVEN_IRQn);
	
	NVIC_EnableIRQ(TIMER0_IRQn);
	NVIC_EnableIRQ(TIMER1_IRQn);
	NVIC_EnableIRQ(TIMER3_IRQn);
	
	NVIC_SetPriority(USB_IRQn, 0);
	
	NVIC_SetPriority(GPIO_EVEN_IRQn, 1);
	
	NVIC_SetPriority(TIMER0_IRQn, 1);
	NVIC_SetPriority(TIMER1_IRQn, 1);
	NVIC_SetPriority(TIMER3_IRQn, 1);
	
	/*
	
	
	NVIC_SetPriority(DMA_IRQn, 1);
	
	
	
	NVIC_SetPriority(GPIO_EVEN_IRQn, 4);
	*/
	
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
	    
    // enable interrupts 
    EnableInterrupts();
	
	// start usb
	USB_Init();
	
	// show startup LEDs
	StartupLEDs();
	
	// start radio driver
	RADIO_Init();
	
	//RADIO_EnableTDMA();
	//RADIO_SetMode(RADIO_RX);
	RADIO_SetMode(RADIO_TX);
	
	uint32_t itr = 0;
	while (1)
	{
		
		RADIO_Main();
		uint8_t packet[32];
		if (RADIO_Recv(packet))
		{
			static char msg[32];
			USB_Transmit((uint8_t*)"RECV\n",5);
		}
		
		if (itr++ % 100000 == 0)
		{
			static char msg[32];
			//USB_Transmit((uint8_t*)"NOP \n",5);
			USB_Transmit((uint8_t*)"SEND\n",5);
			RADIO_Send(packet);
		}
		
		// if no pending irqs, sleep
		// (use energy micro tip of disabling interrupts before
		// enabling after wake up - irq still wake cpu even if disabled)
		//EMU_EnterEM1();
		
	}
	
}
