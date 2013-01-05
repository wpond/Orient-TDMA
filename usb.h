#ifndef __USB_H
#define __USB_H

#include <stdint.h>

void USB_Init(void);
bool USB_Transmit(uint8_t *buf, int len);

#endif // __USB_H
