
#include "efm32.h"

#include "efm32_chip.h"
#include "efm32_rtc.h"
#include "efm32_gpio.h"
#include "efm32_cmu.h"
#include "efm32_timer.h"
#include "efm32_int.h"
#include "efm32_usb.h"
#include "efm32_dma.h"

#include <stdint.h>
#include <stdbool.h>

#include "led.h"
#include "dma.h"
#include "radio.h"
#include "usb.h"
#include "config.h"

void initClocks();
void enableTimers();
void enableInterrupts();

int main()
{
	
	// Chip errata
	CHIP_Init();
	
	// ensure core frequency has been updated
	SystemCoreClockUpdate();
	
	// start clocks
	initClocks();
	
	// start usb
	USB_Init();
	
	// init LEDs
	LED_Init();
	
	// start up LED
	LED_On(RED);
	
	// start DMA
	DMA_Init_TypeDef dmaInit =
    {
        .hprot = 0,
        .controlBlock = dmaControlBlock,
    };
    DMA_Init(&dmaInit);
	
	// start radio
	RADIO_Init();
	
	// enable interrupts
	enableInterrupts();
	
	// start up complete
	LED_Off(RED);
	LED_On(GREEN);
	
	while(1);
	
}

void enableInterrupts()
{
	
	NVIC_EnableIRQ(USART0_TX_IRQn);
	NVIC_EnableIRQ(USART0_RX_IRQn);
	
	NVIC_EnableIRQ(GPIO_EVEN_IRQn);
	
	NVIC_EnableIRQ(TIMER0_IRQn);
	NVIC_EnableIRQ(TIMER1_IRQn);
	
	TRACE("INTERRUPTS ENABLED\n");
	
}

void initClocks()
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
	
	// enable dma
	CMU_ClockEnable(cmuClock_DMA, true);
	
	// enable timers
	CMU_ClockEnable(cmuClock_TIMER0, true);
	CMU_ClockEnable(cmuClock_TIMER1, true);
	
}
