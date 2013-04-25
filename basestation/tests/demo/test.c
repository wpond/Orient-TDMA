
#include <stdio.h>
#include <string.h>

#include "server.h"

int main(int argc, char *argv[])
{
	printf("start server\n");
	SERVER_Init();
	printf("sending test packet\n");
	char *msg = "hello world";
//	SERVER_Send((uint8_t*)msg,strlen(msg));
	SERVER_SendMsg("hello world\n");
	printf("sent\n");
	SERVER_Teardown();
	printf("done\n");
	fflush(stdout);
	return 0;
}
