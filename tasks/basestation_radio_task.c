
#include "efm32_timer.h"

#include "tasks.h"
#include "system.h"

#include "config.h"
#include "packets.h"
#include "radio_init_task.h"
#include "led.h"

/* variables */

/* prototypes */
void basestation_prepare_pulse_rt();
void basestation_receive_mode_rt();

/* functions */
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
	
	TIMER_TopSet(TIMER1, TDMA_SLOT_COUNT * ((TDMA_SLOT_WIDTH + 2*TDMA_GUARD_PERIOD) * (48000000 / 1024)));
	
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
	
	timerCCInit.cmoa = timerOutputActionNone;
	TIMER_InitCC(TIMER1, 1, &timerCCInit);
	TIMER_CompareSet(TIMER1, 1, (TDMA_GUARD_PERIOD + TDMA_SLOT_WIDTH) * (48000000 / 1024));
	
	timer_cb_table_t callback;
	
	callback.timer = TIMER1;
	callback.flags = TIMER_IF_OF;
	callback.cb = &basestation_prepare_pulse_rt;
	
	TIMER_RegisterCallback(callback);
	
	callback.flags = TIMER_IF_CC1;
	callback.cb = &basestation_receive_mode_rt;
	
	TIMER_RegisterCallback(callback);
	
	TIMER_Init(TIMER1, &timerInit);
	
	while(1)
	{
		int i;
		for (i = 0 ;i  < 100000; i++);
		LED_Toggle(BLUE);
	}
	
}

void basestation_prepare_pulse_rt()
{
	
	RADIO_Enable(OFF);
	RADIO_SetMode(TX);
	LED_On(RED);
	LED_Off(GREEN);
	
	packet_t pulse;
	pulse.header.origin_id = NODE_ID;
	pulse.header.destination_id = 0xFF; // broadcast
	pulse.header.message_type = 0x00; // timing pulse
	pulse.header.sequence_number = 0x00;
	RADIO_Send((uint8_t*)&pulse);
	RADIO_TxBufferFill();
	
}

void basestation_receive_mode_rt()
{
	
	RADIO_Enable(OFF);
	RADIO_SetMode(RX);
	RADIO_Enable(RX);
	LED_Off(RED);
	LED_On(GREEN);
	
}