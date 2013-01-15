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
#include "tdma_scheduler.h"

/* variables */

/* prototypes */
void InitClocks();
void HandleInterrupt();
void StartupLEDs();
void EnableInterrupts();
void wait(uint32_t ms);

/* interrupts */
void GPIO_EVEN_IRQHandler()
{
	
	while (GPIO_IntGet() & (1 << NRF_INT_PIN))
	{
		
		GPIO_IntClear((1 << NRF_INT_PIN));
		
		TRACE("GPIO EVEN IRQ\n");
		
		RADIO_IRQHandler();
		
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
	
	// enable dma clock
	CMU_ClockEnable(cmuClock_DMA, true);
	DMA_Reset();
	
	// enable timers 
	CMU_ClockEnable(cmuClock_TIMER0, true);
	CMU_ClockEnable(cmuClock_TIMER1, true);
	TIMER_Reset(TIMER0);
	TIMER_Reset(TIMER1);
	
}

void EnableInterrupts()
{
	
	NVIC_EnableIRQ(GPIO_EVEN_IRQn);
	
	NVIC_EnableIRQ(TIMER0_IRQn);
	NVIC_EnableIRQ(TIMER1_IRQn);
	
	NVIC_SetPriority(USB_IRQn, 3);
	NVIC_SetPriority(DMA_IRQn, 2);
	
	NVIC_SetPriority(TIMER0_IRQn, 0);
	NVIC_SetPriority(TIMER1_IRQn, 1);
	
	NVIC_SetPriority(GPIO_EVEN_IRQn, 0);
	
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
	
	RADIO_EnableSystemCalls(false);
	RADIO_SetMode(RADIO_OFF);
	
	#ifdef SENDER
		
		
		/*
		tdmaConfig.master = true;
		tdmaConfig.channel = 102;
		tdmaConfig.slot = 0;
		tdmaConfig.slotCount = 3;
		tdmaConfig.guardPeriod = 4000;
		tdmaConfig.transmitPeriod = 8000;
		tdmaConfig.protectionPeriod = 4000;
		*/
		
		uint8_t i = 0;
		
		TDMA_Config tdmaConfig;
		
		tdmaConfig.master = true;
		tdmaConfig.channel = 102;
		tdmaConfig.slot = 0;
		tdmaConfig.slotCount = 40;
		tdmaConfig.guardPeriod = 234;
		tdmaConfig.transmitPeriod = 937;
		tdmaConfig.protectionPeriod = 117;
		
		TDMA_Init(&tdmaConfig);
		TDMA_Enable(true);
		
		//RADIO_EnableSystemCalls(true);
		//RADIO_SetMode(RADIO_TX);
		
		while(1)
		{
			
			
			int k;
			for (k = 0; k < 3; k++)
			{
				sprintf(tmsg, "packet queued [0x%X]\n", i);
				memset(packet,i++,32);
				//TRACE(tmsg);
				RADIO_Send(packet);
			}
			wait(200);
			while(RADIO_Recv(packet))
			{
				sprintf(tmsg, "%i: packet recvd [0x%X]\n", TIMER_CounterGet(TIMER1), packet[0]);
				TRACE(tmsg);
			}
			
		}
		
	#else
		
		
		TDMA_Config tdmaConfig;
	
		tdmaConfig.master = false;
		tdmaConfig.channel = 102;
		tdmaConfig.slot = 1;
		tdmaConfig.slotCount = 40;
		tdmaConfig.guardPeriod = 234;
		tdmaConfig.transmitPeriod = 937;
		tdmaConfig.protectionPeriod = 117;
		
		TDMA_Init(&tdmaConfig);
		
		TDMA_Enable(true);
		
		//RADIO_EnableSystemCalls(true);
		//RADIO_SetMode(RADIO_RX);
		
		TRACE("READY\n");
		
		while(1)
		{
			if (RADIO_Recv(packet))
			{
				//TRACE("Packet received\n\n");
				USB_Transmit(packet,1);
				//TRACE("\n\n");
				RADIO_Send(packet);
			}
			TDMA_CheckSync();
		}
		
	#endif
	
	while (1);
	
}
