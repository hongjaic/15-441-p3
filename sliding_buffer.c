/**
 * CS 15-441 Computer Networks
 * Project: Congestion Control with BitTorrent
 *
 * @file    sliding_buffer.c
 * @author  Hong Jai Cho <hongjaic>, Raul Gonzalez <rggonzal>
 */

#include "sliding_buffer.h"


/**
 * remove_head - 
 * This function removes the first element in the sliding window.
 *
 * @param sb    slding window
 */
void remove_head(sliding_buffer_t *sb)
{
    buffer_entry_t *tmp;

    if (sb == NULL)
    {
        return;
    }

    if (sb->head == NULL)
    {
        return;
    }

    tmp = sb->head;
    sb->head = sb->head->next;
    free(tmp->data_packet);
    free(tmp);

    sb->num_entry--;
}


/**
 * append_entry -
 * This function appends entry to the tail of the sliding window.
 *
 * @param sb    sliding window
 * @param ts    time stamp of dp
 * @param dp    packet to append to sliding window
 */
void append_entry(sliding_buffer_t *sb, time_t ts, data_packet_t *dp)
{
    buffer_entry_t *new;

    if (sb == NULL)
    {
        return;
    }

    new = (buffer_entry_t *)malloc(sizeof(buffer_entry_t));
    new->time_sent = ts;
    new->data_packet = dp;
    new->next = NULL;

    if (sb->head == NULL)
    {
        sb->head = new;
        sb->tail = new;
    }
    else
    {
        sb->tail->next = new;
        sb->tail = new;
    }

    sb->num_entry++;
}


/**
 * insert_entry -
 * This function inserts entry into the sliding window so that the sliding
 * window maintains an ascending order of entries. The order is based on the
 * sequence/ack number.
 *
 * @param sb    sliding window
 * @param ts    time stamp of dp
 * @param dp    packet to insert to sliding window
 */
void insert_entry(sliding_buffer_t *sb, time_t ts, data_packet_t *dp)
{
    buffer_entry_t *new;
    buffer_entry_t *prev;
    buffer_entry_t *curr;
    uint32_t new_seq;
    uint32_t prev_seq;
    uint32_t curr_seq;

    if (sb == NULL)
    {
        return;
    }

    new = (buffer_entry_t *)malloc(sizeof(buffer_entry_t));
    new->time_sent = ts;
    new->data_packet = dp;
    new->next = NULL;

    new_seq = new->data_packet->header.seq_num;

    if (sb->head == NULL)
    {
        sb->head = new;
        sb->tail = new;
        sb->num_entry++;
        return;
    }

    curr = sb->head;

    if (new_seq < curr->data_packet->header.seq_num)
    {
        free(new);
        return;
    }

    while(curr != NULL)
    {
        prev = curr;
        prev_seq = prev->data_packet->header.seq_num;
        curr = curr->next;

        if (new_seq == prev_seq)
        {
            free(new->data_packet);
            free(new);
            return;
        }

        if (new_seq > prev_seq)
        {
            if (curr == NULL)
            {
                prev->next = new;
                sb->tail = new;
                sb->num_entry++;
                return;
            }

            curr_seq = curr->data_packet->header.seq_num;

            if (new_seq < curr_seq)
            {
                new->next = curr;
                prev->next = new;
                sb->num_entry++;
                return;
            }
        }
    }
}


/**
 * destroy_entries -
 * This function gets rid of all entries in sliding window/
 *
 * @param sb    sliding window
 */
void destroy_entries(sliding_buffer_t *sb)
{
    buffer_entry_t *prev;
    buffer_entry_t *curr;

    if (sb == NULL)
    {
        return;
    }

    if (sb->head == NULL)
    {
        return;
    }

    curr = sb->head;

    while (curr != NULL)
    {
        prev = curr;
        curr = curr->next;

        free(prev->data_packet);
        free(prev);
    }

    sb->head = NULL;
    sb->tail = NULL;
    sb->num_entry = 0;
}


/**
 * find_ack -
 * This function finds the ack from the receive sliding window. The ack is found
 * such that it is the greatest number of a consecutive sequence. While the ack
 * is being found, all nodes that are visited are removed.
 *
 * @param sb    sliding window
 * @return  ack number, -1 if there are no entries
 */
int find_ack(sliding_buffer_t *sb)
{
    int ack;
    buffer_entry_t *prev;
    buffer_entry_t *curr;

    if (sb == NULL)
    {
        return -1;
    }

    if (sb->head == NULL)
    {
        return -1;
    }

    curr = sb->head;
    ack = curr->data_packet->header.seq_num;
    
    while (curr->next != NULL)
    {
        if (curr->next->data_packet->header.seq_num == ack + 1)
        {
            ack = curr->next->data_packet->header.seq_num;
        }
        else
        {
            return ack;
        }

        prev = curr;
        curr = curr->next;
        free(prev->data_packet);
        free(prev);
        sb->num_entry--;
        sb->head = curr;
    }

    return ack;
}


/**
 * destroy_upto_ack -
 * This function removes all the entries in the send sliding window upto the ack
 * received.
 *
 * @param sb    sliding window
 * @param ack   ack number
 */
void destroy_upto_ack(sliding_buffer_t *sb, int ack)
{
    buffer_entry_t *prev;
    buffer_entry_t *curr;

    if (sb == NULL)
    {
        return;
    }

    if (sb->head == NULL)
    {
        return;
    }

    curr = sb->head;
    
    while (curr != NULL)
    {
        if (ack < curr->data_packet->header.seq_num)
        {
            return;
        }

        prev = curr;
        curr = curr->next;
        free(prev->data_packet);
        free(prev);
        sb->num_entry--;
        sb->head = curr;
        
        if (curr == NULL)
        {
            sb->tail = NULL;
        }
    }
}
