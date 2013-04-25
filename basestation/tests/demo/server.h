#include <stdbool.h>
#include <stdint.h>

bool SERVER_Init();
void SERVER_Send(const uint8_t *payload, uint8_t len);
void SERVER_SendMsg(char *msg);
void SERVER_Teardown();
