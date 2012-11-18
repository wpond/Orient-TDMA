
#include "system.h"
#include "tasks.h"

#include "radio_init_task.h"
#include "led.h"
#include "config.h"
#include "packets.h"

/* variables */
bool syncd = false;

/* prototypes */
void node_sync_timers();
void node_enable_sync();
void node_enable_tx();
void node_config_rx();
void node_disable_autoRefil();

/* functions */
void node_radio_task_entrypoint()
{
	
	// enable timer 0
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
	
	TIMER_TopSet(TIMER0, TDMA_SLOT_COUNT * ((TDMA_SLOT_WIDTH + 2*TDMA_GUARD_PERIOD) * (48000000 / 1024)));
	
	// CC input to capture time sync packet
	TIMER_InitCC_TypeDef timerCCInit = 
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
  
  // enable CC2#2 input
  TIMER0->ROUTE |= (TIMER_ROUTE_CC2PEN | TIMER_ROUTE_LOCATION_LOC2);
	
	timer_cb_table_t callback;
	
	callback.timer = TIMER0;
	callback.flags = TIMER_IF_CC2;
	callback.cb = node_sync_timers;
	
	TIMER_RegisterCallback(&callback);
	
	TIMER_InitCC(TIMER0, 2, &timerCCInit);
	
	TIMER_Init(TIMER0, &timerInit);
	TIMER_Init(TIMER1, &timerInit);
	
	// enable rx
	RADIO_Enable(OFF);
	RADIO_SetMode(RX);
	RADIO_Enable(RX);
	
	while(!syncd);
	
	// finish configuration
	
	// on overflow enable receive (receive for entire guard period also)
	callback.timer = TIMER0;
	callback.flags = TIMER_IF_OF;
	callback.cb = node_enable_sync;
	
	TIMER_RegisterCallback(&callback);
	
	// change to tx
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
	callback.cb = node_enable_tx;
	
	TIMER_RegisterCallback(&callback);
	
	TIMER_CompareSet(TIMER0, 0, (NODE_ID * (2*TDMA_GUARD_PERIOD) + TDMA_SLOT_WIDTH) * (48000000 / 1024));
	TIMER_InitCC(TIMER0, 0, &timerCCInitComp);
	
	callback.timer = TIMER0;
	callback.flags = TIMER_IF_CC1;
	callback.cb = node_config_rx;
	
	TIMER_RegisterCallback(&callback);
	
	TIMER_CompareSet(TIMER0, 1, ((NODE_ID+1) * ((2*TDMA_GUARD_PERIOD) + TDMA_SLOT_WIDTH)) * (48000000 / 1024));
	TIMER_InitCC(TIMER0, 1, &timerCCInitComp);
	
	callback.timer = TIMER1;
	callback.flags = TIMER_IF_CC1;
	callback.cb = node_disable_autoRefil;
	
	TIMER_RegisterCallback(&callback);
	
	TIMER_CompareSet(TIMER1, 1, ((NODE_ID * ((2*TDMA_GUARD_PERIOD) + TDMA_SLOT_WIDTH)) + TDMA_GUARD_PERIOD + TDMA_SLOT_WIDTH - 0.0005) * (48000000 / 1024));
	TIMER_InitCC(TIMER1, 1, &timerCCInitComp);
	
	while(1);
	
}

void node_disable_autoRefil()
{
	
	RADIO_SetAutoRefil(false);
	
}

void node_config_rx()
{
	
	RADIO_Enable(OFF);
	RADIO_SetMode(RX);
	
}

void node_enable_tx()
{
	
	RADIO_Enable(OFF);
	RADIO_SetMode(TX);
	RADIO_SetAutoRefil(true);
	
}

void node_enable_sync()
{
	
	RADIO_Enable(RX);
	
}

void node_sync_timers()
{
	
	uint8_t packet[32];
	
	if (!RADIO_Recv(packet))
		return;
	
	if (packet[1] == 0xFF && packet[2] == 0x00)
	{
		
		// get packet IRQ time
		int32_t time = TIMER_CaptureGet(TIMER0, 2);
		
		// subtract known offset
		time -= (TDMA_GUARD_PERIOD + (0.5*TDMA_SLOT_WIDTH)) * (48000000 / 1024);
		
		// if time is less than zero, add max value of timer (uint16_t)
		if (time < 0)
			time += 65535;
		
		// add timer difference between now and input
		int32_t diff = TIMER_CounterGet(TIMER0) - time;
		
		// if counter has wrapped
		if (diff < 0)
		{
			diff = (65535 - time) + TIMER_CounterGet(TIMER0);
		}
		
		TIMER_CounterSet(TIMER0, diff);
		TIMER_CounterSet(TIMER1, diff);
		
		syncd = true;
		
	}
	
}
