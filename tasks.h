#ifndef __TASKS_H__
#define __TASKS_H__

#include "scheduler.h"

/* tasks */
task_t radio_init_task;

/* entry points */
void radio_init_task_entrypoint();

#endif