#ifndef __TRACE_H__
#define __TRACE_H__

#include <stdint.h>
#include <stdbool.h>

static inline void TRACE(char *msg)
{
	USB_Transmit((uint8_t*)msg, strlen(msg));
}

#endif