#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <stdint.h>
#include <stdbool.h>

#define IN_USE_FLAG				0x00000001
#define EXEC_FLAG					0x00000002
#define USB_FLAG					0x00000004
#define USART0_FLAG				0x00000008
#define USART1_FLAG				0x00000010
#define USART2_FLAG				0x00000020

#define MAX_TASKS 				32
#define RT_QUEUE_SIZE 		4

#define TASK_STACK_SIZE 	1024
#define TASK_DURATION 		240000 // 5ms (200hz)

typedef struct 
{
	
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr;
	uint32_t pc;
	uint32_t psr;
	
} hw_stack_frame_t;

typedef struct 
{

	uint32_t r4;
	uint32_t r5;
	uint32_t r6;
	uint32_t r7;
	uint32_t r8;
	uint32_t r9;
	uint32_t r10;
	uint32_t r11;
	
} sw_stack_frame_t;

typedef struct
{
	
	uint8_t stack_start[TASK_STACK_SIZE];
	void *stack;
	sw_stack_frame_t sw_stack_frame;
	
} task_t;

typedef struct
{
	
	task_t *task;
	uint32_t flags;
	
} task_table_t;

void SCHEDULER_Init();
bool SCHEDULER_TaskInit(task_t *task, void *entry_point);
void SCHEDULER_Run();
void SCHEDULER_Wait(uint32_t flags);
void SCHEDULER_Release(uint32_t flags);
void SCHEDULER_Yield();
void SCHEDULER_RunRTTask(void (*task)(void));

#endif