#include "sliding_buffer.h"

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
        //new->next = sb->head;
        //sb->head = new;
        //sb->num_entry++;
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

/*int have_seqnum(sliding_buffer_t *sb, int seqnum)
{
    buffer_entry_t *head;
    
    if (sb == NULL)
    {
        return -1;
    }

    if (sb->head == NULL);
    {
        return -1;
    }

    curr = sb->head;
}*/

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
