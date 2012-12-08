#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string.h>

#include "usb.h"

static void inline TRACE(char* msg)
{
    USB_Transmit((uint8_t*)msg,strlen(msg));
}

#define NODE_ID 1

#endif