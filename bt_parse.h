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
#include <stdint.h>
#include "packets.h"
#include "constants.h"


void bt_init(bt_config_t *c, int argc, char **argv);
void bt_parse_command_line(bt_config_t *c);
void bt_parse_peer_list(bt_config_t *c);
void bt_dump_config(bt_config_t *c);
bt_peer_t *bt_peer_info(const bt_config_t *c, int peer_id);
bt_peer_t *bt_peer_info2(const bt_config_t *c, struct sockaddr_in * from);
#endif /* _BT_PARSE_H_ */
