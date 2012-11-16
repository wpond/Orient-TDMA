#ifndef __PACKETS_H__
#define __PACKETS_H__

#include <stdint.h>

typedef enum
{
	
	timing_pulse = 0,
	session_config = 1,
	magnetometer_calibration = 2,
	start_stop = 3,
	device_available = 4,
	capture_data_header = 5,
	capture_data = 6,
	acknowledgement = 7
	
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
	typedef union
	{
		
	} packet_payload;

} packet_t;

#endif