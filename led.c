#include "led.h"

#include "efm32_gpio.h"

LED_Config leds[3];

void LED_Init()
{

	// set up leds
	LED_Config led;
	
	led.pin = 15;
	led.port = 0;
	leds[0] = led;
	GPIO_PinModeSet(led.port, led.pin, gpioModeWiredAnd, 1);
	
	led.pin = 15;
	led.port = 4;
	leds[1] = led;
	GPIO_PinModeSet(led.port, led.pin, gpioModeWiredAnd, 1);
	
	led.pin = 14;
	led.port = 4;
	leds[2] = led;
	GPIO_PinModeSet(led.port, led.pin, gpioModeWiredAnd, 1);
	
}

void LED_On(LED led)
{	
	GPIO->P[leds[led].port].DOUT &= ~(1 << leds[led].pin);
}

void LED_Off(LED led)
{
	GPIO->P[leds[led].port].DOUT |= (1 << leds[led].pin);
}

void LED_Toggle(LED led)
{
	GPIO->P[leds[led].port].DOUT ^= (1 << leds[led].pin);
}
