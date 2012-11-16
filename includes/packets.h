#ifndef __PACKETS_H__
#define __PACKETS_H__

#include <stdint.h>

typedef enum
{
	
	TIMING_PULSE = 0,
	SESSION_CONFIG = 1,
	MAGNETOMETER_CALIBRATION = 2,
	START_STOP = 3,
	DEVICE_AVAILABLE = 4,
	CAPTURE_DATA_HEADER = 5,
	CAPTURE_DATA = 6,
	ACKNOWLEDGEMENT = 7
	
} packet_type_e;

typedef struct
{
	
	uint8_t origin_id,
		destination_id,
		message_type,
		sequence_number;
	uint32_t send_time;
	
} packet_header_t;



typedef struct
{

	packet_header_t header;
	uint8_t packet_payload[24];

} packet_t;

#endif