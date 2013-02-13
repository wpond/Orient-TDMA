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
#include "tdma.h"
#include "packets.h"
#include "aloha.h"

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
	
	NVIC_SetPriority(USB_IRQn, 0);
	NVIC_SetPriority(DMA_IRQn, 1);
	
	NVIC_SetPriority(TIMER0_IRQn, 2);
	NVIC_SetPriority(TIMER1_IRQn, 2);
	
	NVIC_SetPriority(GPIO_EVEN_IRQn, 3);
	
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
	
	ALOHA_Enable(true);
	
	#ifdef BASESTATION
		
		TRACE("BASESTATION");
		
		uint8_t packet[32];
		PACKET_Raw *rawPacket = (PACKET_Raw*)packet;
		
		while (1)
		{
			
			if (USB_Recv(packet))
			{
				// intercept messages for basestation node
				if (rawPacket->addr == NODE_ID)
				{
					TRACE("Incoming packet:\n");
					TRACESTRUCT(rawPacket,32);
					RADIO_HandleIncomingPacket(rawPacket);
				}
				else
				{
					RADIO_Send(packet);
				}
			}
			
			if (RADIO_Recv(packet))
			{
				USB_Transmit(packet,32);
			}
			
		}
		
	#else
		
		TRACE("NODE\n");
		
		uint8_t packet[32];
		
		while (1)
		{
			
			if (RADIO_Recv(packet))
				RADIO_Send(packet);
			
			TDMA_CheckSync();
			
		}
		
	#endif
	
	
	/*
	TDMA_Config tdmaConfig;
	
	#ifdef SENDER
		tdmaConfig.master = true;
		tdmaConfig.slot = 0;
	#else
		tdmaConfig.master = false;
		tdmaConfig.slot = 1;
	#endif
	// tdmaConfig.slot = NODE_ID;
	tdmaConfig.slotCount = 40;
	tdmaConfig.guardPeriod = 234;
	tdmaConfig.transmitPeriod = 937;
	tdmaConfig.protectionPeriod = 117;
	
	TDMA_Init(&tdmaConfig);
	TDMA_Enable(true);
	
	#ifdef SENDER
		
		TRACE("BASESTATION\n");
		
		while (1)
		{
			
			PACKET_Raw hello;
		
			hello.addr = 0xFF;
			hello.type = PACKET_HELLO;
			
			PACKET_PayloadHello *helloPayload = (PACKET_PayloadHello*)hello.payload;
			
			helloPayload->challengeResponse = HELLO_CHALLENGE;
			
			RADIO_Send((uint8_t*)&hello);
			wait(1000);
			RADIO_Recv((uint8_t*)&hello);
						
			wait(2000);
			
		}
		
	#else
		
		TRACE("NODE\n");
		
		uint8_t packet[32];
		
		while (1)
		{
			
			RADIO_Recv(packet);
			TDMA_CheckSync();
			
		}
		
	#endif
	*/
	
}
