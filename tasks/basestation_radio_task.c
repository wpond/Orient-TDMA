
#include "efm32_timer.h"

#include "tasks.h"
#include "system.h"

#include "radio_init_task.h"
#include "led.h"

/* variables */

/* prototypes */
void basestation_prepare_pulse_rt();
void basestation_receive_mode_rt();

/* functions */
void TIMER1_IRQHandler()
{
	
	uint32_t irq = TIMER_IntGet(TIMER1);
	
	if (irq & TIMER_IF_OF)
	{
		
		LED_Toggle(RED);
		SCHEDULER_RunRTTask(&basestation_prepare_pulse_rt);
		
		TIMER_IntClear(TIMER1, TIMER_IF_OF);
	}
	
	if (irq & TIMER_IF_CC0)
	{
		
		
		
		TIMER_IntClear(TIMER1, TIMER_IF_CC0);
	}
	
	if (irq & TIMER_IF_CC1)
	{
		
		//SCHEDULER_RunRTTask(&basestation_receive_mode_rt);
		
		TIMER_IntClear(TIMER1, TIMER_IF_CC1);
	}
	
}

void basestation_radio_task_entrypoint()
{
	
	// initialise
	basestation_prepare_pulse_rt();
	
	// set up CC irqs
	TIMER_Init_TypeDef timerInit =
	{
		.enable     = true, 
		.debugRun   = true, 
		.prescale   = timerPrescale1024, 
		.clkSel     = timerClkSelHFPerClk, 
		.fallAction = timerInputActionNone, 
		.riseAction = timerInputActionNone, 
		.mode       = timerModeUp, 
		.dmaClrAct  = false,
		.quadModeX4 = false, 
		.oneShot    = false, 
		.sync       = false, 
	};
	
	TIMER_TopSet(TIMER1, 255 * ((TDMA_SLOT_WIDTH + 2*TDMA_GUARD_PERIOD) * (48000000 / 1024)));
	
	TIMER_InitCC_TypeDef timerCCInit = 
	{
		.cufoa      = timerOutputActionNone,
		.cofoa      = timerOutputActionNone,
		.cmoa       = timerOutputActionSet,
		.mode       = timerCCModeCompare,
		.filter     = true,
		.prsInput   = false,
		.coist      = false,
		.outInvert  = false,
	};
	
	TIMER1->ROUTE |= (TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC4); 
	
	TIMER_InitCC(TIMER1, 0, &timerCCInit);
	TIMER_CompareSet(TIMER1, 0, TDMA_GUARD_PERIOD * (48000000 / 1024));
	
	TIMER_IntEnable(TIMER1, TIMER_IF_CC0);
	
	TIMER_Init(TIMER1, &timerInit);
	
	while(1);
	
}

void basestation_prepare_pulse_rt()
{
	
	RADIO_Enable(OFF);
	RADIO_SetMode(TX);
	
	uint8_t payload[32];
	RADIO_Send(payload);
	RADIO_TxBufferFill();
	
}

void basestation_receive_mode_rt()
{
	
	RADIO_Enable(OFF);
	RADIO_SetMode(RX);
	RADIO_Enable(RX);
	
}