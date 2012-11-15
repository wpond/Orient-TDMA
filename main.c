
#include "efm32.h"

#include "efm32_chip.h"
#include "efm32_rtc.h"
#include "efm32_gpio.h"
#include "efm32_cmu.h"
#include "efm32_timer.h"
#include "efm32_int.h"
#include "efm32_usb.h"

#include <stdint.h>
#include <stdbool.h>

#include "scheduler.h"
#include "tasks.h"
#include "system.h"
#include "led.h"

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
		
	// init LEDs
	LED_Init();
	
	// init usb
	USB_Init();
	
	// init scheduler
	SCHEDULER_Init();
	
	// enable interrupts
	enableInterrupts();
	
	// init tasks
	SCHEDULER_TaskInit(&radio_task, radio_task_entrypoint);
	
	// run
	SCHEDULER_Run();
	
}

void enableInterrupts()
{
	
	NVIC_EnableIRQ(SysTick_IRQn);
	NVIC_EnableIRQ(PendSV_IRQn);
	
	NVIC_EnableIRQ(USART0_TX_IRQn);
	NVIC_EnableIRQ(USART0_RX_IRQn);
	
	NVIC_EnableIRQ(GPIO_EVEN_IRQn);
	
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
	
}
