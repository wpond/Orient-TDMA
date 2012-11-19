#ifndef __TASKS_H__
#define __TASKS_H__

#include "scheduler.h"

/* tasks */
task_t radio_init_task,
	basestation_radio_task,
	node_radio_task,
	usb_relay_task;

/* entry points */
void radio_init_task_entrypoint();
void basestation_radio_task_entrypoint();
void node_radio_task_entrypoint();
void usb_relay_task_entrypoint();

#endif