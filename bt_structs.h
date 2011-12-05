#ifndef _BT_STRUCTS_H_
#define _BT_STRUCTS_H_

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "constants.h"
#include "packets.h"
#include "sliding_buffer.h"

typedef struct bt_peer_s {
	unsigned short  id;
    uint32_t num_chunks;
    hehas_t *hehas;
	uint32_t chunks_fetched;
	uint32_t last_ack;
    uint32_t last_seq;
    uint32_t num_dupacks;
	get_packet_t *his_request;
	uint32_t send_hash_id;
    uint32_t get_hash_id;
	struct sockaddr_in addr;
	struct bt_peer_s *next;

    fetching_or_fetched_t *fetching_or_fetched;

    struct bt_peer_s *curr_to;

    whohas_list_t whohas_list;
    whohas_packet_t * pending_whohas;
    ihave_packet_t *pending_ihave;
    get_packet_t *pending_get;
    
    uint32_t num_retransmits;
    uint32_t num_data_retransmits;
    uint32_t ack_timeout_counter;

    sliding_buffer_t recv_buffer;
} bt_peer_t;



struct bt_config_s {
  char  chunk_file[BT_FILENAME_LEN];
  char  has_chunk_file[BT_FILENAME_LEN];
  char  output_file[BT_FILENAME_LEN];
  char  peer_list_file[BT_FILENAME_LEN];
  int   max_conn;
  unsigned short identity;
  unsigned short myport;

  int argc; 
  char **argv;

  bt_peer_t *peers;
};
typedef struct bt_config_s bt_config_t;

#endif /* _BT_STRUCTS_H_ */
