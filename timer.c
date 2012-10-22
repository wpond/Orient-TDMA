/*

rxen - pe12 - TIM1_CC2 #1

ce - pd6 - TIM1_CC0 #4, LETIM0_OUT0 #0

irq - pf8 - TIM0_CC2 #2

*/
#include "timer.h"

#include "stdbool.h"

#include "efm32.h"
#include "efm32_system.h"
#include "efm32_timer.h"
#include "efm32_cmu.h"
#include "efm32_int.h"

#include "node_config.h"

#include "radio.h"
#include "led.h"

void TDMATIMER_Init()
{
	
	TIMER_Init_TypeDef timerInit =
  {
    .enable     = true, 
    .debugRun   = true, 
    .prescale   = timerPrescale1, 
    .clkSel     = timerClkSelHFPerClk, 
    .fallAction = timerInputActionNone, 
    .riseAction = timerInputActionNone, 
    .mode       = timerModeUp, 
    .dmaClrAct  = false,
    .quadModeX4 = false, 
    .oneShot    = false, 
    .sync       = false, 
  };
	
	// enable timers
	/*
	#ifdef NODE
	TIMER_IntEnable(TIMER0, TIMER_IF_CC2);
	
	TIMER_Init(TIMER0, &timerInit);
	
	NVIC_ClearPendingIRQ(TIMER0_IRQn);
	NVIC_EnableIRQ(TIMER0_IRQn);
	#endif
	*/
	
	LED_On(RED);
	
	TIMER_IntClear(TIMER1, TIMER_IF_OF);
	NVIC_ClearPendingIRQ(TIMER1_IRQn);
	
	LED_On(BLUE);
	
	TIMER_IntEnable(TIMER1, TIMER_IF_OF);
	NVIC_EnableIRQ(TIMER1_IRQn);
	
	LED_On(GREEN);
	
	TIMER_TopSet(TIMER1, CMU_ClockFreqGet(cmuClock_TIMER1));
	
	TIMER_Init(TIMER1, &timerInit);
	
}

void TDMATIMER_ConfigBS()
{
	
	return;
	// enable CE every NRF_TIMESLOTS_COUNT * NRF_TIMESLOTS_LENGTH
	// ce - pd6 - TIM1_CC0 #4, LETIM0_OUT0 #0
	
	/* Select CC channel parameters */
  TIMER_InitCC_TypeDef timerCCInit = 
  {
    .cufoa      = timerOutputActionNone,
    .cofoa      = timerOutputActionSet,
    .cmoa       = timerOutputActionNone,
    .mode       = timerCCModeCompare,
    .filter     = true,
    .prsInput   = false,
    .coist      = false,
    .outInvert  = false,
  };
	
	 /* Configure CC channel 0 */
  TIMER_InitCC(TIMER1, 0, &timerCCInit);

  /* Route CC0 to location 3 (PD1) and enable pin */  
  TIMER1->ROUTE |= (TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC4); 
  
  /* Set Top Value */
  //TIMER_TopSet(TIMER1, NRF_TIMESLOT_COUNT * NRF_TIMESLOT_LENGTH); 
  TIMER_TopSet(TIMER1, CMU_ClockFreqGet(cmuClock_TIMER1));
	
}

void TDMATIMER_ConfigNode()
{
	
	
	
}
