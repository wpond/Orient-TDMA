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

#include <stdint.h>
#include <stdbool.h>

#include "alloc.h"
#include "led.h"
#include "radio.h"
#include "usb.h"
#include "config.h"
#include "packets.h"

/* variables */
static uint32_t rtcIrqCount = 0;

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

void RTC_IRQHandler()
{
	
	uint32_t flags = RTC_IntGet();
	
	if (flags & RTC_IF_OF || flags & RTC_IF_COMP0)
	{
		rtcIrqCount += dataRate;
	}
	
	RTC_IntClear(flags);
	
}

/* functions */
void wait(uint32_t ms)
{
	
	int i; for (i = 0; i < 5000*ms; i++); return;
	
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
	RTC_Init_TypeDef rtcInit = RTC_INIT_DEFAULT;
	RTC_CompareSet(0, 1638);
	RTC_IntEnable(RTC_IFC_COMP0);
	RTC_Init(&rtcInit);
	
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
	
	NVIC_EnableIRQ(RTC_IRQn);
	
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
	//#ifdef BASESTATION
		USB_Init();
	//#endif
	
	// show startup LEDs
	StartupLEDs();
	
	// start radio driver
	RADIO_Init();
	
	//RADIO_EnableTDMA();
	
	uint8_t packet[32];
	
	int i;
	uint8_t dPacket[5*25];
	for (i = 0; i < 5*25; i++)
	{
		dPacket[i] = i;
	}
	
	i = 0;
	while (1)
	{
		
		RADIO_Main();
		
		#ifdef BASESTATION
			
			if (USB_Recv(packet))
			{
				
				if (packet[0] == BASESTATION_ID)
				{
					switch (packet[1])
					{
					case PACKET_TDMA_TIMING:
						break;
					case PACKET_HELLO:
						if (packet[2] == 0xFF)
						{
							packet[2] = BASESTATION_ID;
							USB_Transmit(packet,32);
						}
						break;
					case PACKET_TDMA_CONFIG:
					{
						PACKET_TDMA *packetTDMA = (PACKET_TDMA*)&packet[2];
						RADIO_TDMAConfig *c = (RADIO_TDMAConfig*)packetTDMA->payload;
						RADIO_ConfigTDMA(c);
						
						#ifdef BASESTATION
							ALLOC_Init(packetTDMA->payload[sizeof(RADIO_TDMAConfig)+3],packetTDMA->payload[sizeof(RADIO_TDMAConfig)+1]);
							ALLOC_SetLease(packetTDMA->payload[sizeof(RADIO_TDMAConfig)+2]);
						#endif
						RADIO_BasestationReset();
						
						if (packetTDMA->payload[sizeof(RADIO_TDMAConfig)])
						{
							RADIO_EnableTDMA();
						}
						else
						{
							RADIO_DisableTDMA();
						}
						break;
					}
					case PACKET_TDMA_ENABLE:
						if (packet[3])
						{
							RADIO_EnableTDMA();
						}
						else
						{
							RADIO_DisableTDMA();
						}
						break;
					case PACKET_TDMA_SLOT:
					case PACKET_TDMA_ACK:
					case PACKET_TRANSPORT_DATA:
					case PACKET_TRANSPORT_ACK:
					case PACKET_EVENT:
						break;
					}
				}
				else
				{
					RADIO_Send(packet);
				}
				
			}
			
		#else
			
			if (rtcIrqCount > 0)
			{
				if (rtcIrqCount > 5)
				{
					RADIO_SendData(dPacket,25*5);
					INT_Disable();
					rtcIrqCount -= 5;
					INT_Enable();
				}
				else
				{
					RADIO_SendData(dPacket,25);
					INT_Disable();
					rtcIrqCount--;
					INT_Enable();
				}
			}
			else if (!dataRate)
			{
				RADIO_SendData(dPacket,85);
			}
			/*
			if (i++ % 100000 == 0)
			{
				RADIO_SendData(dPacket,85);
				TRACE("send data packet\n");
			}
			*/
			
		#endif
		
		// if no pending irqs, sleep
		// (use energy micro tip of disabling interrupts before
		// enabling after wake up - irq still wake cpu even if disabled)
		//EMU_EnterEM1();
		
	}
	
}
