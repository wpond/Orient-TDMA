#ifndef __USB_H
#define __USB_H

extern volatile int USB_Message;

int USB_Setup(void);
int USB_Transmit(uint8_t *buf, int len);

#endif // __USB_H
