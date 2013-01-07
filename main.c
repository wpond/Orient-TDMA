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
	
	NVIC_SetPriority(USB_IRQn, 0);
	NVIC_SetPriority(DMA_IRQn, 1);
	
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
	
	//#define SENDER
	
	uint8_t packet[32];
	char tmsg[255];
	
	RADIO_EnableSystemCalls(true);
	
	#ifdef SENDER
		RADIO_SetMode(RADIO_TX);
		uint32_t i = 0;
		int j;
		while(1)
		{
			i++;
			memcpy(&packet[0],&i,sizeof(uint32_t));
			while (!RADIO_Send(packet));
			if (i % 1000 == 0)
			{
				sprintf(tmsg,"time = %i\n", RTC_CounterGet());
				TRACE(tmsg);
				LED_Toggle(RED);
			}
		}
	#else
		RADIO_SetMode(RADIO_RX);
		uint32_t i = 0, tmp;
		uint16_t miss_count = 0;
		uint32_t recv_count = 0;
		LED_On(BLUE);
		while(1)
		{
			if (RADIO_Recv(packet))
			{
				memcpy(&tmp,&packet[0],sizeof(uint32_t));
				//sprintf(tmsg,"%i\n", tmp);
				//TRACE(tmsg);
				recv_count++;
				if (i != tmp)
				{
					sprintf(tmsg,"missed packet [%i,%i]\n", i, tmp);
					//TRACE(tmsg);
					LED_Toggle(BLUE);
					miss_count += tmp - i;
					i = tmp;
				}
				i++;
				if (recv_count % 1000 == 0)
				{
					sprintf(tmsg,"time = %i, recv count = %i, miss count = %i\n", RTC_CounterGet(), recv_count, miss_count);
					TRACE(tmsg);
					recv_count = 0;
					miss_count = 0;
					LED_Toggle(GREEN);
				}
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
