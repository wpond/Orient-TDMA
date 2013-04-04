#ifndef __USB_H
#define __USB_H

#include <stdint.h>
#include <stdbool.h>

#define USB_RECV_SIZE 4

void USB_Init(void);
bool USB_Transmit(uint8_t *buf, int len);
bool USB_Recv(uint8_t packet[32]);

#endif // __USB_H
