#include "node_radio_task.h"

#include "system.h"
#include "tasks.h"

#include "radio_init_task.h"
#include "led.h"
#include "config.h"
#include "packets.h"

/* variables */
bool sync_available = false;
uint32_t sync_count = 0;
int32_t sync_irq_time;

/* prototypes */
void node_enable_sync();
void node_config_tx();
void node_config_rx();
void node_disable_autoRefil();
void node_enable_tx();
void node_cc_time();

/* functions */
void node_radio_task_entrypoint()
{
	
	TIMER_Reset(TIMER0);
	TIMER_Reset(TIMER1);
	
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
	TIMER_TopSet(TIMER1, TDMA_SLOT_COUNT * ((TDMA_SLOT_WIDTH + 2*TDMA_GUARD_PERIOD) * (48000000 / 1024)));

	// CC input to capture time sync packet
	TIMER_InitCC_TypeDef timerCCInit = 
  {
		.eventCtrl  = timerEventFalling,
		.edge       = timerEdgeFalling,
		.cufoa      = timerOutputActionNone,
		.cofoa      = timerOutputActionNone,
		.cmoa       = timerOutputActionNone,
		.mode       = timerCCModeCapture,
		.filter     = false,
		.prsInput   = false,
		.coist      = false,
		.outInvert  = false,
  };
  
  // enable CC2#2 input
  TIMER0->ROUTE |= (TIMER_ROUTE_CC2PEN | TIMER_ROUTE_LOCATION_LOC2);

	timer_cb_table_t callback;

	callback.timer = TIMER0;
	callback.flags = TIMER_IF_CC2;
	callback.cb = node_cc_time;

	TIMER_RegisterCallback(&callback);
	
	TIMER_InitCC(TIMER0, 2, &timerCCInit);

	TIMER_Init(TIMER0, &timerInit);
	TIMER_Init(TIMER1, &timerInit);

	// enable rx
	RADIO_Enable(OFF);
	RADIO_SetMode(RX);
	RADIO_Enable(RX);
	
	sync_available = true;
	
	while(sync_count < 1);
	
	TIMER_InitCC_TypeDef timerCCInitTx = 
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
	
	callback.timer = TIMER1;
	callback.flags = TIMER_IF_CC0;
	callback.cb = node_enable_tx;

	TIMER_RegisterCallback(&callback);
	
	TIMER_CompareSet(TIMER1, 0, (NODE_ID * (TDMA_SLOT_WIDTH + 2*TDMA_GUARD_PERIOD) + TDMA_GUARD_PERIOD) * (48000000 / 1024));
	TIMER_InitCC(TIMER1, 0, &timerCCInitTx);
	
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
	
	callback.timer = TIMER1;
	callback.flags = TIMER_IF_CC2;
	callback.cb = node_enable_sync;

	TIMER_RegisterCallback(&callback);
	
	TIMER_CompareSet(TIMER1, 2, 100);
	TIMER_InitCC(TIMER1, 2, &timerCCInitComp);
	
	callback.timer = TIMER0;
	callback.flags = TIMER_IF_CC0;
	callback.cb = node_config_tx;

	TIMER_RegisterCallback(&callback);

	TIMER_CompareSet(TIMER0, 0, (TDMA_SLOT_WIDTH + 2*TDMA_GUARD_PERIOD) * (48000000 / 1024));
	TIMER_InitCC(TIMER0, 0, &timerCCInitComp);

	callback.timer = TIMER0;
	callback.flags = TIMER_IF_CC1;
	callback.cb = node_config_rx;

	TIMER_RegisterCallback(&callback);

	TIMER_CompareSet(TIMER0, 1, ((NODE_ID+1) * ((2*TDMA_GUARD_PERIOD) + TDMA_SLOT_WIDTH)) * (48000000 / 1024) - 1);
	TIMER_InitCC(TIMER0, 1, &timerCCInitComp);

	callback.timer = TIMER1;
	callback.flags = TIMER_IF_CC1;
	callback.cb = node_disable_autoRefil;
	
	TIMER_RegisterCallback(&callback);
	
	TIMER_CompareSet(TIMER1, 1, ((NODE_ID * ((2*TDMA_GUARD_PERIOD) + TDMA_SLOT_WIDTH)) + TDMA_GUARD_PERIOD + TDMA_SLOT_WIDTH- 0.005) * (48000000 / 1024));
	TIMER_InitCC(TIMER1, 1, &timerCCInitComp);
	
	uint8_t p[32];
	uint8_t j = 0;
	int i;
	while(1)
	{
		
		memset(p,j++,32);
		for (i = 0; i < 100000; i++);
		if (RADIO_Send(p))
			TRACE("QUEUE PACKET\n");
		
	}
	
}

void node_enable_tx()
{
	
	char tmsg[255];
	sprintf(tmsg, "%i: node_enable_tx()\n", TIMER_CounterGet(TIMER1));
	TRACE(tmsg);
	
}

void node_disable_autoRefil()
{

	RADIO_SetAutoRefil(false);
	
	char tmsg[255];
	sprintf(tmsg, "%i: node_disable_autoRefil()\n", TIMER_CounterGet(TIMER1));
	TRACE(tmsg);
	
}

void node_config_rx()
{
	
	RADIO_Enable(OFF);
	RADIO_SetMode(RX);
	
	char tmsg[255];
	sprintf(tmsg, "%i: node_config_rx()\n", TIMER_CounterGet(TIMER0));
	TRACE(tmsg);
	
}

void node_config_tx() // possible bug here
{

	RADIO_Enable(OFF);
	RADIO_SetMode(TX);
	RADIO_SetAutoRefil(true);
	RADIO_TxBufferFill();
	
	char tmsg[255];
	sprintf(tmsg, "%i: node_config_tx()\n", TIMER_CounterGet(TIMER0));
	TRACE(tmsg);

}

void node_enable_sync()
{
	
	sync_available = true;
	
	RADIO_Enable(RX);
	
	char tmsg[255];
	sprintf(tmsg, "%i: node_enable_sync()\n", TIMER_CounterGet(TIMER0));
	TRACE(tmsg);
	
}

void node_sync_timers()
{
	
	// get packet IRQ time
	int32_t t = TIMER_CounterGet(TIMER0), 
		time = sync_irq_time;
	
	char tmsg[255];
	sprintf(tmsg, "%i: node_sync_timers() : sync_available: 0x%X;\n", t, sync_available);
	TRACE(tmsg);
	
	if (!sync_available)
	{
		return;
	}
	
	int32_t remote_time = (TDMA_GUARD_PERIOD * (48000000 / 1024));
	
	int32_t time_since_recv = TIMER_CounterGet(TIMER0) - sync_irq_time;
	if (time_since_recv < 0)
	{
		time_since_recv = (TIMER_TopGet(TIMER0) - sync_irq_time) + TIMER_CounterGet(TIMER0);
	}
	
	remote_time += time_since_recv;
	
	if (remote_time < 0)
		remote_time += TIMER_TopGet(TIMER0);
	if (remote_time > TIMER_TopGet(TIMER0))
		remote_time -= TIMER_TopGet(TIMER0);
	
	TIMER_CounterSet(TIMER0, remote_time);
	TIMER_CounterSet(TIMER1, remote_time);
	
	sprintf(tmsg, "SYNC(%i) IRQ_TIME: %i; TIMER0: %i; REMOTE_TIMER: %i;\n", sync_count, sync_irq_time, t, remote_time);
	TRACE(tmsg);
	
	sync_count++;
	sync_available = false;
	
}

void node_cc_time()
{
	
	char tmsg[255];
	sprintf(tmsg, "%i: NRF IRQ LOW\n", TIMER_CounterGet(TIMER1));
	TRACE(tmsg);
	
	sync_irq_time = TIMER_CaptureGet(TIMER0,2);
	
}

void node_set_sync_time()
{
	
	char tmsg[255];
	sprintf(tmsg, "%i: STORE SYNC TIME (%i)\n", TIMER_CounterGet(TIMER1), sync_irq_time);
	TRACE(tmsg);
	
}
