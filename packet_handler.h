#ifndef _PACKET_HANDLER_H_
#define _PACKET_HANDLER_H_

#include <math.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>

#include "constants.h"
#include "packets.h"
#include "sliding_buffer.h"
#include "congestion_avoidance.h"
#include "hexhashlib.h"

extern bt_config_t config;
extern local_chunks_t local_chunks;
extern master_chunks_t master_chunks;
extern char master_data_file[BT_FILENAME_LEN];
extern char request_chunks_file[BT_FILENAME_LEN];
extern sliding_buffer_t recv_buffer;
extern sliding_buffer_t send_buffer;
extern uint32_t cwnd;
extern congestion_state_t state;

extern bt_peer_t *me;

void packet_handler( int sock,void *peer,void *packet);
void *bytes_to_packet(char *buf, int size);
void send_whohas(void *peers,int id, int num_chunks, char *all_chunks);

#endif /* _PACKET_HANDLER_H_ */
