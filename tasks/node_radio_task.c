#include "node_radio_task.h"

#include "system.h"
#include "tasks.h"

#include "radio_init_task.h"
#include "led.h"
#include "config.h"
#include "packets.h"

/* variables */
uint32_t sync_count = 0;
int32_t sync_irq_time;

#define NUM_MODES 4
typedef enum
{
	
	CE_OFF_PRE_RX = 0,
	CE_RX = 1,
	CE_OFF_PRE_TX = 2,
	CE_TX = 3,
	
} node_radio_modes;

node_radio_modes mode = CE_TX;

/* prototypes */
void node_store_irq_time_rt();
void node_config_ce_pin_rt();
void node_t0_cc0_rt();
void node_t0_cc1_rt();
void node_t0_of_rt();
void node_t1_cc1_rt();

/* functions */
void node_radio_task_entrypoint()
{
	
	TIMER_Reset(TIMER0);
	TIMER_Reset(TIMER1);
	
	TIMER_TopSet(TIMER0, TDMA_SLOT_COUNT * ((TDMA_SLOT_WIDTH + 2*TDMA_GUARD_PERIOD) * (48000000 / 1024)));
	TIMER_TopSet(TIMER1, TDMA_SLOT_COUNT * ((TDMA_SLOT_WIDTH + 2*TDMA_GUARD_PERIOD) * (48000000 / 1024)));
	
	// enable timers 0 and 1
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
	
	TIMER_Init(TIMER0, &timerInit);
	TIMER_Init(TIMER1, &timerInit);
	
	// enable CC for timer sync
	TIMER_InitCC_TypeDef timerCCInitCapture = 
	{
		.eventCtrl  = timerEventFalling,
		.edge       = timerEdgeFalling,
		.cufoa      = timerOutputActionNone,
		.cofoa      = timerOutputActionNone,
		.cmoa       = timerOutputActionNone,
		.mode       = timerCCModeCapture,
		.filter     = true,
		.prsInput   = false,
		.coist      = false,
		.outInvert  = false,
	};
	
	TIMER0->ROUTE |= (TIMER_ROUTE_CC2PEN | TIMER_ROUTE_LOCATION_LOC2);
	
	timer_cb_table_t callback;

	callback.timer = TIMER0;
	callback.flags = TIMER_IF_CC2;
	callback.cb = node_store_irq_time_rt;

	TIMER_RegisterCallback(&callback);

	TIMER_InitCC(TIMER0, 2, &timerCCInitCapture);
	
	RADIO_Enable(OFF);
	RADIO_SetMode(RX);
	RADIO_Enable(RX);
	
	// wait for 5 sync messages
	while (sync_count < 5);
	
	RADIO_Enable(OFF);
	
	TIMER1->ROUTE |= (TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC4);
	
	callback.timer = TIMER1;
	callback.flags = TIMER_IF_CC0;
	callback.cb = node_config_ce_pin_rt;
	
	TIMER_RegisterCallback(&callback);
	
	node_config_ce_pin_rt();
	
	TIMER_InitCC_TypeDef timerCCInitComp = 
	{
		.cufoa      = timerOutputActionNone,
		.cofoa      = timerOutputActionNone,
		.cmoa       = timerOutputActionNone,
		.mode       = timerCCModeCompare,
		.filter     = true,
		.prsInput   = false,
		.coist      = false,
		.outInvert  = false,
	};
	
	callback.timer = TIMER0;
	callback.flags = TIMER_IF_CC0;
	callback.cb = node_t0_cc0_rt;

	TIMER_RegisterCallback(&callback);

	TIMER_CompareSet(TIMER0, 0, (2*TDMA_GUARD_PERIOD + TDMA_SLOT_WIDTH) * (48000000 / 1024));
	TIMER_InitCC(TIMER0, 0, &timerCCInitComp);
	
	callback.timer = TIMER0;
	callback.flags = TIMER_IF_CC1;
	callback.cb = node_t0_cc1_rt;
	
	TIMER_RegisterCallback(&callback);

	TIMER_CompareSet(TIMER0, 1, (NODE_ID * (2*TDMA_GUARD_PERIOD + TDMA_SLOT_WIDTH) + TDMA_GUARD_PERIOD + TDMA_SLOT_WIDTH) * (48000000 / 1024));
	TIMER_InitCC(TIMER0, 1, &timerCCInitComp);
	
	callback.timer = TIMER0;
	callback.flags = TIMER_IF_OF;
	callback.cb = node_t0_of_rt;

	TIMER_RegisterCallback(&callback);
	
	callback.timer = TIMER1;
	callback.flags = TIMER_IF_CC1;
	callback.cb = node_t1_cc1_rt;

	TIMER_RegisterCallback(&callback);

	TIMER_CompareSet(TIMER1, 1, (NODE_ID * (2*TDMA_GUARD_PERIOD + TDMA_SLOT_WIDTH) + TDMA_GUARD_PERIOD + 0.2*TDMA_SLOT_WIDTH) * (48000000 / 1024));
	TIMER_InitCC(TIMER1, 1, &timerCCInitComp);
	
	uint8_t p[32],
		j = 0;
	int i;
	char tmsg[255];
	while(1)
	{
		
		memset(p,j++,32);
		for (i = 0; i < 1000000; i++);
		RADIO_Send(p);
		sprintf(tmsg,"%i: Send 0x%2.2X\n", TIMER_CounterGet(TIMER0), j);
		TRACE(tmsg);
		
	}
	
}

void node_t0_cc0_rt()
{
	
	RADIO_Enable(OFF);
	RADIO_SetMode(TX);
	RADIO_SetAutoRefil(true);
	RADIO_TxBufferFill();
	
	char tmsg[255];
	sprintf(tmsg, "%i: node_t0_cc0_rt(): set mode tx\n", TIMER_CounterGet(TIMER0));
	TRACE(tmsg);
	
}

void node_t0_cc1_rt()
{
	
	RADIO_Enable(OFF);
	RADIO_SetMode(RX);
	
	char tmsg[255];
	sprintf(tmsg, "%i: node_t0_cc1_rt(): set mode rx\n", TIMER_CounterGet(TIMER0));
	TRACE(tmsg);
	
}

void node_t0_of_rt()
{
	
	RADIO_Enable(RX);
	
	char tmsg[255];
	sprintf(tmsg, "%i: node_t0_of_rt(): enable rx\n", TIMER_CounterGet(TIMER0));
	TRACE(tmsg);
	
}

void node_t1_cc1_rt()
{
	
	RADIO_SetAutoRefil(false);
	
	char tmsg[255];
	sprintf(tmsg, "%i: node_t1_cc1_rt(): disable auto refil\n", TIMER_CounterGet(TIMER0));
	TRACE(tmsg);
	
}

// set next time and value of pin
void node_config_ce_pin_rt()
{
	
	TIMER_InitCC_TypeDef timerCCInit = 
	{
		.cufoa      = timerOutputActionNone,
		.cofoa      = timerOutputActionNone,
		.cmoa       = timerOutputActionNone,
		.mode       = timerCCModeCompare,
		.filter     = true,
		.prsInput   = false,
		.coist      = false,
		.outInvert  = false,
	};
	
	int32_t next_time;
	char tmsg[255];
	
	switch (mode)
	{
	case CE_OFF_PRE_RX:
		next_time = 0;
		timerCCInit.cmoa = timerOutputActionSet;
		sprintf(tmsg, "%i: node_config_ce_pin_rt(): SET (RX): %i\n", TIMER_CounterGet(TIMER0), 0);
		break;
	case CE_RX:
		next_time = (TDMA_SLOT_WIDTH + 2*TDMA_GUARD_PERIOD) * (48000000 / 1024);
		timerCCInit.cmoa = timerOutputActionClear;
		sprintf(tmsg, "%i: node_config_ce_pin_rt(): CLEAR (RX): %i\n", TIMER_CounterGet(TIMER0), next_time);
		break;
	case CE_OFF_PRE_TX:
		next_time = ((NODE_ID) * (TDMA_SLOT_WIDTH + 2*TDMA_GUARD_PERIOD) + TDMA_GUARD_PERIOD) * (48000000 / 1024);
		timerCCInit.cmoa = timerOutputActionSet;
		sprintf(tmsg, "%i: node_config_ce_pin_rt(): SET (TX): %i\n", TIMER_CounterGet(TIMER0), next_time);
		break;
	case CE_TX:
		next_time = ((NODE_ID) * (TDMA_SLOT_WIDTH + 2*TDMA_GUARD_PERIOD) + TDMA_GUARD_PERIOD + TDMA_SLOT_WIDTH) * (48000000 / 1024);
		timerCCInit.cmoa = timerOutputActionClear;
		sprintf(tmsg, "%i: node_config_ce_pin_rt(): CLEAR (TX): %i\n", TIMER_CounterGet(TIMER0), next_time);
		break;
	}
	
	TRACE(tmsg);
	
	// move to next mode
	mode = (mode + 1) % NUM_MODES;
	
	TIMER_CompareSet(TIMER1, 0, next_time);
	TIMER_InitCC(TIMER1, 0, &timerCCInit);
	
}

void node_store_irq_time_rt()
{
	
	sync_irq_time = TIMER_CaptureGet(TIMER0, 2);
	
}

// called by radio on recept of timing pulse
void node_sync_timers_rt()
{
	
	// get packet IRQ time
	int32_t time = sync_irq_time,
		old_time = TIMER_CounterGet(TIMER0);
	
	// subtract known offset
	time -= TDMA_GUARD_PERIOD * (48000000 / 1024);
	
	// if time is less than zero, add max value of timer (uint16_t)
	if (time < 0)
		time += TIMER_TopGet(TIMER0);
	
	// add timer difference between now and input
	int32_t diff = TIMER_CounterGet(TIMER0) - time;
	
	// if counter has wrapped
	if (diff < 0)
	{
		diff = (TIMER_TopGet(TIMER0) - time) + TIMER_CounterGet(TIMER0);
	}
	
	TIMER_CounterSet(TIMER0, diff);
	TIMER_CounterSet(TIMER1, diff);
	
	sync_count++;
	
	char tmsg[255];
	sprintf(tmsg, "%i: node_sync_timers_rt(): calculate new timer values (old = %i; new = %i;)\n", TIMER_CounterGet(TIMER0), old_time, diff);
	TRACE(tmsg);
	
}

