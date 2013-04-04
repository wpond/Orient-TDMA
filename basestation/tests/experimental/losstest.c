#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "Windows.h"

#include "rs232.h"
#include "packets.h"

#define COMPORT 4

/* variables */
typedef struct
{
	bool enabled;
	int hits,
		misses,
		bytes,
		frame,
		seg;
	float dataRate;
}
NodeInfo_t;

/* variables */
static NodeInfo_t nodeInfo[255];
static bool countPackets = true;

/* prototypes */
static void DisableNode(uint8_t nodeid);
static void EnableMaster(uint8_t nodeCount, int txp, uint8_t slots, uint8_t lease);
static void EnableNode(uint8_t nodeid, int txp, uint8_t loss, bool optimisations, uint16_t dataRate, uint8_t slotCount);
static int RunTest(uint16_t time, uint8_t nodeCount, int txp, uint8_t loss, bool optimisations, uint16_t dataRate, uint8_t slots, uint8_t lease,bool spreadspeed);
static void HandlePacket(uint8_t packet[32]);
static void Recv(uint16_t ms);

/* functions */
int main(int argsc, char *argsv[])
{

	int i, j;
	int testId, repeats, time, duration, nodeCount, txp, loss, optimisations, dataRate, slots, lease, spreadspeed;
	int linePos = 0;
	char line[256],
		c;
	
	if (RS232_OpenComport(COMPORT,1000000))
	{
		printf("Unable to open COM%i\n",COMPORT+1);
		return 1;
	}
	
	FILE *input = fopen("input.csv","r"), 
		*output = fopen("results.csv","w");
	
	sprintf(line,"test id,repeat,node id,duration (s),node count, transmit period, packet loss, optimisations, data generation rate (kbps), unreserved slots, lease time, spread speed, throughput (kbps), accepted packets, dropped packets\n");
	fwrite(line,1,strlen(line),output);
	
	while (!feof(input))
	{
		c = fgetc(input);
		line[linePos++] = c;
		if (c == '\n')
		{
			line[linePos] = '\0';
			if (line[0] != '#')
			{
				sscanf(line,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",&testId,&repeats,&time,&nodeCount,&txp,&loss,&optimisations,&dataRate,&slots,&lease,&spreadspeed);
				
				for (i = 0; i < repeats; i++)
				{
					duration = RunTest(time,nodeCount,txp,loss,optimisations,dataRate,slots,lease,spreadspeed);
					
					printf("time elapsed: %i\n",duration);
					for (j = 0; j < 256; j++)
					{
						if (nodeInfo[j].enabled)
						{
							printf("node %i:\n\thits: %i\n\tmisses: %i\n\tbytes: %i\n\tkbps: %f\n", j, nodeInfo[j].hits, nodeInfo[j].misses, nodeInfo[j].bytes, ((float)(nodeInfo[j].bytes * 8) / 1024.0) / (float)(duration / 1000.0f));
							sprintf(line,"%d,%d,%d,%f,%d,%d,%d,%d,%d,%f,%d,%d,%f,%d,%d\n",testId,i,j,duration/1000.0f,nodeCount,txp,loss,optimisations,spreadspeed,nodeInfo[j].dataRate,slots,lease,((float)(nodeInfo[j].bytes * 8) / 1024.0) / (float)(duration / 1000.0f),nodeInfo[j].hits,nodeInfo[j].misses);
							fwrite(line,1,strlen(line),output);
						}
					}
					fflush(output);
				}
			}
			linePos = 0;
		}
	}
	RS232_CloseComport(COMPORT);
	
	fclose(input);
	fclose(output);
	
	return 0;
}

static int RunTest(uint16_t time, uint8_t nodeCount, int txp, uint8_t loss, bool optimisations, uint16_t dataRate, uint8_t slots, uint8_t lease,bool spreadspeed)
{
	int i;
	
	for (i = 0; i < 256; i++)
		nodeInfo[i].enabled = false;
	
	int increment = loss / nodeCount,
		curLoss = increment;
	
	for (i = 1; i < nodeCount+1; i++)
	{
		EnableNode(i, txp, curLoss, optimisations, dataRate, nodeCount + slots + 1);
		nodeInfo[i].loss = curLoss;
		curLoss += increment;
		Recv(100);
	}
	EnableMaster(nodeCount + 1, txp, slots, lease);
	countPackets = false;
	
	uint8_t buf[4096];
	for (i = 0; i < 2000; i++)
	{
		Sleep(1);
		while (RS232_PollComport(COMPORT, buf, 4096)); // clear the internal buffer
	}
	
	countPackets = true;
	int duration = GetTickCount();
	Recv(time * 1000);
	duration = GetTickCount() - duration;
	DisableNode(0xFF);
	Recv(500);
	DisableNode(0);
	Recv(500);
	
	return duration;
	
}

static void EnableNode(uint8_t nodeid, int txp, uint8_t loss, bool optimisations, uint16_t dataRate, uint8_t slotCount)
{
	
	PACKET_Raw p;
	p.addr = nodeid;
	p.type = PACKET_TDMA_CONFIG;
	
	RADIO_TDMAConfig *config = (RADIO_TDMAConfig*)(p.payload+1);
	
	config->master = false;
	config->channel = 102;
	config->slot = nodeid;
	config->slotCount = slotCount;
	config->guardPeriod = 100;
	config->transmitPeriod = txp;
	config->protectionPeriod = 50;
	
	// fill in extra settings
	uint8_t *ptr = p.payload + sizeof(RADIO_TDMAConfig) + 1;
	ptr[0] = 1; // enable
	ptr[1] = loss; // loss rate
	ptr[2] = (optimisations) ? 1 : 0; // improvements
	ptr[3] = dataRate & 0xFF; // data rate
	ptr[4] = (dataRate >> 8) & 0xFF; // data rate
	
	printf("enable node [nid=%i,txp=%i,loss=%i,optimisations=%x,dataRate=%i,slots=%i]\n",nodeid,txp,loss,optimisations,dataRate,slotCount);
	
	RS232_SendBuf(COMPORT,(unsigned char *)&p,32);
	
}

static void EnableMaster(uint8_t nodeCount, int txp, uint8_t slots, uint8_t lease)
{
	
	PACKET_Raw p;
	p.addr = 0;
	p.type = PACKET_TDMA_CONFIG;
	
	RADIO_TDMAConfig *config = (RADIO_TDMAConfig*)(p.payload+1);
	
	config->master = true;
	config->channel = 102;
	config->slot = 0;
	config->slotCount = slots + nodeCount;
	config->guardPeriod = 100;
	config->transmitPeriod = txp;
	config->protectionPeriod = 50;
	
	// fill in extra settings
	uint8_t *ptr = p.payload + sizeof(RADIO_TDMAConfig) + 1;
	ptr[0] = 1; // enable
	ptr[1] = slots; // slots
	ptr[2] = lease; // lease
	ptr[3] = nodeCount; // nodes
	
	printf("enable master [txp=%i,reservedSlots=%i,unreservedSlots=%i,slotCount=%i,lease=%i]\n",txp,nodeCount,slots,slots + nodeCount,lease);
	
	RS232_SendBuf(COMPORT,(unsigned char *)&p,32);
	
}

static void DisableNode(uint8_t nodeid)
{
	
	PACKET_Raw p;
	p.addr = nodeid;
	p.type = PACKET_TDMA_ENABLE;
	p.payload[0] = 0;
	p.payload[1] = 0;
	
	RS232_SendBuf(COMPORT,(unsigned char *)&p,32);
	
}

static void Recv(uint16_t ms)
{
	
	uint8_t buf[4096 + 32];
	int i, j, count, offset = 0;
	
	for (i = 0; i < ms; i++)
	{
		count = RS232_PollComport(COMPORT, &buf[offset], 4096);
		offset += count;
		j = 0;
		while (offset >= 32)
		{
			HandlePacket(&buf[j]);
			offset -= 32;
			j += 32;
		}
		if (offset > 0)
			printf("offset: %i ",offset);
		memcpy(buf,&buf[count],offset);
		if (count < 4096)
			Sleep(1);
	}
	
}

static void HandlePacket(uint8_t packet[32])
{
	
	NodeInfo_t *n;
	
	switch (packet[1])
	{
	case PACKET_HELLO:
		printf("HELLO from 0x%2.2X\n",packet[2]);
		break;
	case PACKET_TRANSPORT_DATA:
		n = &nodeInfo[packet[2]];
		if (n->enabled)
		{
			if (packet[3] == n->frame && packet[4] == n->seg)
			{
				if (countPackets)
				{
					n->hits++;
					n->bytes += (int)packet[5];
					if (packet[6] & TRANSPORT_FLAG_SEGMENT_END)
					{
						n->frame = (packet[3] + 1) % 256;
						n->seg = 0;
					}
					else
					{
						n->frame = packet[3];
						n->seg = (packet[4] + 1) % 256;
					}
				}
				
			}
			else
			{
				if (countPackets)
				{
					n->misses++;
				}
			}
		}
		else
		{
			if (countPackets)
			{
				n->hits = 1;
				n->misses = 0;
				n->bytes = (int)packet[5];
				n->enabled = true;
			}
			else
			{
				n->hits = 0;
				n->misses = 0;
				n->bytes = 0;
			}
			if (packet[6] & TRANSPORT_FLAG_SEGMENT_END)
			{
				n->frame = (packet[3] + 1) % 256;
				n->seg = 0;
			}
			else
			{
				n->frame = packet[3];
				n->seg = (packet[4] + 1) % 256;
			}
		}
		break;
	case 0xFF:
	{
		uint8_t len = packet[2];
		char msg[30];
		memcpy(msg,&packet[4],len);
		msg[len] = 0;
		printf(msg);
	}
		break;
	}
	
}
