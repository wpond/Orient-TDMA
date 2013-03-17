#ifndef __USB_H__

	#define __USB_H__
	
	#include <stdint.h>
	#include <stdbool.h>
	
	#include "usb/usbd.h"
	
	static inline void USB_Transmit(uint8_t *d, uint16_t l) {}
	static inline bool USB_Recv(uint8_t *d) { return false; }
	
#endif