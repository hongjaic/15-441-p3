/**
 * CS 15-441 Computer Networks
 * Project: Congestion Control with BitTorrent
 *
 * This file defines the data structures and fuctions for the sliding window
 * used for ocngestion control.
 *
 * @file    sliding_buffer.h
 * @author  Hong Jai Cho <hongjaic>, Raul Gonzalez <rggonzal>
 */

#ifndef _SLIDING_BUFFER_H_
#define _SLIDING_BUFFER_H_

#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include "packets.h"

/* Sliding window entry */
typedef struct buffer_entry_s
{
    time_t time_sent;
    data_packet_t *data_packet;
    struct buffer_entry_s *next;
} buffer_entry_t;

/* Sliding window data structure */
typedef struct sliding_buffer_s
{
    uint32_t num_entry;
    buffer_entry_t *head;
    buffer_entry_t *tail;
} sliding_buffer_t;


void remove_head(sliding_buffer_t *sb);
void append_entry(sliding_buffer_t *sb, time_t ts, data_packet_t *dp);
void insert_entry(sliding_buffer_t *sb, time_t ts, data_packet_t *dp);
void destroy_entries(sliding_buffer_t *sb);
int find_ack(sliding_buffer_t *sb);
void destroy_upto_ack(sliding_buffer_t *sb, int ack);

#endif /* _SLIDING_BUFFER_H_ */
