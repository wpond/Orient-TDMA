#include "server.h"

#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <winsock.h>

static SOCKET         s;
static SOCKADDR_IN    recipient;

bool SERVER_Init()
{
	WSADATA        wsd;
    
    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
    {
        printf("WSAStartup failed!\n");
        return false;
    }
    // Create the socket
    //
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET)
    {
        printf("socket() failed; %d\n", WSAGetLastError());
        return false;
    }
    // Resolve the recipient's IP address or hostname
    //
    recipient.sin_family = AF_INET;
    recipient.sin_port = htons((short)9899);
    if ((recipient.sin_addr.s_addr = inet_addr("127.0.0.1"))
		== INADDR_NONE)
    {
        struct hostent *host=NULL;

        host = gethostbyname("127.0.0.1");
        if (host)
            memcpy(&recipient.sin_addr, host->h_addr_list[0],
                host->h_length);
        else
        {
            printf("gethostbyname() failed: %d\n", WSAGetLastError());
            WSACleanup();
            return false;
        }
    }
    
     if (connect(s, (SOCKADDR *)&recipient,
                sizeof(recipient)) == SOCKET_ERROR)
	{
		printf("connect() failed: %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}

    
   return true;
}

void SERVER_Send(const uint8_t *payload, uint8_t len)
{
	int ret;
	ret = ret = send(s, payload, len, 0);
	if (ret == SOCKET_ERROR)
	{
		printf("send() failed; %d\n", WSAGetLastError());
	}
}

void SERVER_SendMsg(char *msg)
{
	SERVER_Send((const uint8_t*)msg,strlen(msg));
}

void SERVER_Teardown()
{
	closesocket(s);
    WSACleanup();
}
