/**
 * CS 15-441 Computer Networks
 *
 * Struct definition for an WHOHAS packet.
 *
 * @file    packets.h
 * @author  Raul Gonzalez <rggonzal>,Hong Jai Cho <hongjaic>
 */

#ifndef _PACKETS_H_
#define _PACKETS_H_

#include "constants.h"

#include <math.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

typedef struct hehas_s
{
    uint32_t chunk_id;
    struct hehas_s *next;
    uint32_t chunkhash[5];
} hehas_t;

typedef struct local_chunks_s
{
    uint32_t num_chunks;
    hehas_t *hehas;
} local_chunks_t;

typedef struct master_chunks_s
{
    uint32_t num_chunks;
    hehas_t *hehas;
} master_chunks_t;

#pragma pack (1)
typedef struct header_s {
  unsigned short magicnum;
  char version;
  char packet_type;
  unsigned short header_len;
  unsigned short packet_len; 
  uint32_t seq_num;
  uint32_t ack_num;
} header_t;  

typedef struct whohas
{
   header_t header;
   uint32_t num_chunks;
   uint32_t chunks[];
} whohas_packet_t;

typedef struct ihave
{
   header_t header;
   uint32_t num_chunks;
   uint32_t chunks[];
} ihave_packet_t;

typedef struct get
{
   header_t header;
   uint32_t hash[5];
} get_packet_t;

typedef struct _data
{
   header_t header;
   unsigned char data[];
} data_packet_t;

typedef struct ack
{
   header_t header;
} ack_packet_t;

typedef struct denied
{
   header_t header;
} denied_packet_t;
#pragma pack()

typedef struct fetching_or_fetched_s
{
    int chunk_id;
    struct fetching_or_fetched_s *next;
} fetching_or_fetched_t;

typedef struct bt_peer_s {
	unsigned short  id;
	//ihave_packet_t *hehas;
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

#endif /* _PACKETS_H_ */

