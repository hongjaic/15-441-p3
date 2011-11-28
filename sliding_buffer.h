#ifndef _SLIDING_BUFFER_H_
#define _SLIDING_BUFFER_H_

#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include "packets.h"

typedef struct buffer_entry_s
{
    time_t time_sent;
    data_packet_t *data_packet;
    struct buffer_entry_s *next;
} buffer_entry_t;

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
