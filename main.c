/* includes */
#include "efm32.h"

#include "efm32_chip.h"
#include "efm32_dma.h"
#include "efm32_rtc.h"
#include "efm32_gpio.h"
#include "efm32_cmu.h"
#include "efm32_timer.h"
#include "efm32_int.h"

#include "stdint.h"
#include "stdbool.h"

#include "config.h"

#include "dma.h"
#include "led.h"
#include "radio.h"

/* variables */

/* prototypes */
void InitClocks();
void HandleInterrupt();
void StartupLEDs();
void EnableInterrupts();
void wait(uint32_t ms);

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

void EnableInterrupts()
{
	
	NVIC_EnableIRQ(GPIO_EVEN_IRQn);
	
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
	
	// start DMA
	DMA_Init_TypeDef dmaInit =
    {
        .hprot = 0,
        .controlBlock = dmaControlBlock,
    };
    DMA_Init(&dmaInit);
    
    // init radio
    RADIO_Init();
    
    // init usb
    USB_Init();
    
    // enable interrupts 
    EnableInterrupts();
	
	// show startup LEDs
	StartupLEDs();
	
	#define TEST1
	
	uint8_t packet[32];
	char tmsg[255];
	
	#ifdef TEST1
		RADIO_SetMode(RADIO_TX);
		int i = 0;
		while(1)
		{
			memset(packet,i++,32);
			while (!RADIO_Send(packet));
			LED_Toggle(RED);
			if (i % 10000 == 0)
			{
				sprintf(tmsg,"time = %i\n", RTC_CounterGet());
				TRACE(tmsg);
			}
		}
	#else
		RADIO_SetMode(RADIO_RX);
		uint8_t i = 0;
		while(1)
		{
			if (RADIO_Recv(packet))
			{
				LED_Toggle(GREEN);
				if (i != packet[0])
				{
					sprintf(tmsg,"missed packet [%i,%i]\n", i, packet[0]);
					TRACE(tmsg);
					i = packet[0];
				}
				i++;
			}
		}
	#endif
	
	while (1);
	
}

void GPIO_EVEN_IRQHandler()
{
	
	if (GPIO_IntGet() & (1 << NRF_INT_PIN))
	{
		
		RADIO_IRQHandler();
		
		GPIO_IntClear((1 << NRF_INT_PIN));
		
	}
	
}
