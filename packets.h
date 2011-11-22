/**
 * CS 15-441 Computer Networks
 *
 * Struct definition for an WHOHAS packet.
 *
 * @file    packets.h
 * @author  Raul Gonzalez <rggonzal>,Hong Jai Cho <hongjaic>
 */

#ifndef PACKETS_H
#define PACKETS_H

#include "bt_parse.h"
#include "constants.h"

#include <math.h>



typedef struct chunks
{
   int num_chunks;
   char hashes[BUFLEN];
}chunks_t;

typedef struct header_s {
  short magicnum;
  char version;
  char packet_type;
  short header_len;
  short packet_len; 
  u_int seq_num;
  u_int ack_num;
} header_t;  

typedef struct whohas
{
   header_t header;
   chunks_t chunks;
}whohas_packet_t;

typedef struct ihave
{
   header_t header;
   chunks_t chunks;
}ihave_packet_t;


typedef struct get
{
   header_t header;
   char hash[HASHLEN];
}get_packet_t;

typedef struct data
{
   header_t header;
   char data[BUFLEN];
}data_packet_t;

typedef struct ack
{
   header_t header;

}ack_packet_t;

typedef struct denied
{
   header_t header;

}denied_packet_t;


void packet_handler(struct sockaddr_in *toaddr,void *packet, chunks_t *local_chunks);
void *bytes_to_packet(char *buf, int size);
void send_whohas(bt_peer_t *peers,int id, int num_chunks, char *all_chunks);
#endif

