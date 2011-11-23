/*
 * bt_parse.h
 *
 * Initial Author: Ed Bardsley <ebardsle+441@andrew.cmu.edu>
 * Class: 15-441 (Spring 2005)
 *
 * Skeleton for 15-441 Project 2 command line and config file parsing
 * stubs.
 *
 */

#ifndef _BT_PARSE_H_
#define _BT_PARSE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "packets.h"

#define BT_FILENAME_LEN 255
#define BT_MAX_PEERS 1024

typedef struct bt_peer_s {
	short  id;
	ihave_packet_t *hehas;
	int chunks_fetched;
	int bytes_received;
	int bytes_sent;
	ack_packet_t *lastack;
	get_packet_t *his_request;
	int hash_id;
	struct sockaddr_in addr;
	struct bt_peer_s *next;
} bt_peer_t;

struct bt_config_s {
  char  chunk_file[BT_FILENAME_LEN];
  char  has_chunk_file[BT_FILENAME_LEN];
  char  output_file[BT_FILENAME_LEN];
  char  peer_list_file[BT_FILENAME_LEN];
  int   max_conn;
  short identity;
  unsigned short myport;

  int argc; 
  char **argv;

  bt_peer_t *peers;
};
typedef struct bt_config_s bt_config_t;


void bt_init(bt_config_t *c, int argc, char **argv);
void bt_parse_command_line(bt_config_t *c);
void bt_parse_peer_list(bt_config_t *c);
void bt_dump_config(bt_config_t *c);
bt_peer_t *bt_peer_info(const bt_config_t *c, int peer_id);
bt_peer_t *bt_peer_info2(const bt_config_t *c, struct sockaddr_in * from);
#endif /* _BT_PARSE_H_ */
