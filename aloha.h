#ifndef __ALOHA_H__

	#define __ALOHA_H__
	
	#include <stdbool.h>
	
	void ALOHA_Enable(bool enable);
	bool ALOHA_IsEnabled();
	bool ALOHA_Send();
	
#endif