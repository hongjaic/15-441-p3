/**
 * CS 15-441 Computer Networks
 * Project: Congestion Control with BitTorrent
 * 
 * @file    packet_handler.c
 * @author  Hong Jai Cho <hongjaic>, Raul Gonzalez <rggonzal>
 */

#include <assert.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "packet_handler.h"


int get_hash_id(char *hash);
int get_hash_id_master(uint32_t *hash);
int get_hash_id_request(uint32_t *hash);

void whohas_handler(int sock, bt_peer_t *peer, whohas_packet_t *whohas);
void ihave_handler(int sock, bt_peer_t *peer, ihave_packet_t *ihave);
void get_handler(int sock, bt_peer_t *peer, get_packet_t *get);
void data_handler(int sock, bt_peer_t *peer, data_packet_t *data_p);
void ack_handler(int sock, bt_peer_t *peer, ack_packet_t *ack);
void denied_handler(int sock,bt_peer_t *peer, denied_packet_t * denied);

void get_next_chunk(int sock, bt_peer_t *peer);
void send_next_data(int sock, bt_peer_t *peer, int hash_id, int seqnum);
void send_next_whohas(int sock, bt_peer_t *peer);

void post_send_cleanup(bt_peer_t *peer);
void post_receive_cleanup(bt_peer_t *peer);

void destroy_fof(bt_peer_t *peer);


/**
 * bytes_to_packet -
 * Allocations memory for received bytes and converts them to
 * a packet.
 *
 * @param buf   received bytes
 * @param size  number of received bytes
 */
void *bytes_to_packet(char *buf, int size)
{
	void *packet = (void *)malloc(size);
	memcpy(packet,buf,size);	
	return packet;
}


/**
 * get_header -
 * Extracts the header of a packet.
 *
 * @param header    data structure that the header is extracted into
 * @param packet    packet to extract header from
 */
void get_header(header_t *header, void *packet)
{
	memcpy(header, packet, HEADERLEN);
}


/**
 * set_header -
 * Sets ths header of a packet.
 *
 * @param header        data structure to copy header values into
 * @param type          type of packet
 * @param seq_num       sequence number of packet
 * @param ack_num       acknowledgement number of packet
 * @param header_len    length of header
 * @param packet_len    length of entire packet
 */
void set_header(header_t *header, int type, int seq_num, int ack_num, int header_len, int packet_len){

	header->magicnum = MAGIC;
	header->version = VERSION;
	header->packet_type = type;
	header->header_len = header_len;
	header->packet_len = packet_len;
	header->seq_num = seq_num;
	header->ack_num = ack_num;	
}


/**
 * create_whohas -
 * Creates a whohas packet given bytes of hash values
 *
 * @param chunks_num    num of chunk hash values
 * @param chunk_hashes  chunk hashes to copy into whohas packet
 * @return whohas packet
 */
whohas_packet_t * create_whohas(int chunks_num, char *chunk_hashes)
{ 
    int i;
    char *ptr;
    uint32_t chunk_hashes_int[chunks_num*HASHLEN_PACKET];

	whohas_packet_t *whohas = (whohas_packet_t *)malloc(HEADERLEN + 4 + chunks_num*HASHLEN_PACKET);
	assert(whohas != NULL);

    for (i = 0; i < chunks_num; i++)
    {
        ptr = chunk_hashes + (i*HASHLEN);
        chunk_hashes_int[i*5] = hash_to_int(ptr, 0, 8);
        chunk_hashes_int[i*5 + 1] = hash_to_int(ptr, 8, 8);
        chunk_hashes_int[i*5 + 2] = hash_to_int(ptr, 16, 8);
        chunk_hashes_int[i*5 + 3] = hash_to_int(ptr, 24, 8);
        chunk_hashes_int[i*5 + 4] = hash_to_int(ptr, 32, 8);
    }

	set_header(&(whohas->header), TYPE_WHOHAS, PAD, PAD, HEADERLEN, HEADERLEN + 4 + chunks_num*HASHLEN_PACKET);
	whohas->num_chunks = chunks_num;
	memcpy((whohas->chunks), chunk_hashes_int, HASHLEN_PACKET*chunks_num);
	
    return whohas;
}


/**
 * create_ihave -
 * Creates an ihave packet given the chunk hashes this peer has.
 * 
 * @param chunk_hits    the chunks hashes that this peer has in integer form
 * @param chunks_num    the number of chunk hashes
 * @return ihave packet
 */
ihave_packet_t * create_ihave(uint32_t *chunk_hits, int chunks_num)
{
	ihave_packet_t *ihave = (ihave_packet_t *)malloc(HEADERLEN + 4 + chunks_num*HASHLEN_PACKET);
	assert(ihave != NULL);
    
    set_header(&(ihave->header), TYPE_IHAVE, PAD, PAD, HEADERLEN, HEADERLEN + 4 + chunks_num*HASHLEN_PACKET);
	ihave->num_chunks = chunks_num;
    memcpy(&(ihave->chunks), chunk_hits, HASHLEN_PACKET*chunks_num);

	return ihave;
}


/**
 * create_get -
 * Creates a get packet given a chunk hash value.
 *
 * @param hash  chunk hash value in integer form
 * @return get packet
 */
get_packet_t * create_get(uint32_t *hash)
{
	get_packet_t *get = (get_packet_t *)malloc(HEADERLEN + HASHLEN_PACKET);
	assert(get != NULL);
    
    set_header(&(get->header), TYPE_GET, PAD, PAD, HEADERLEN, HEADERLEN + HASHLEN_PACKET);
	memcpy(&(get->hash), hash, HASHLEN_PACKET);
	
    return get;
}


/**
 * create_data -
 * Creates a data packet.
 *
 * @param data      data to include in packet
 * @param seqnum    sequence number of data packet
 * @param size      number of bytes of data
 * @return datap packet
 */
data_packet_t * create_data(char *data, int seqnum, int size)
{
	data_packet_t *data_p = (data_packet_t *)malloc(HEADERLEN + size);
	assert(data_p != NULL);
    
    set_header(&(data_p->header), TYPE_DATA, seqnum, PAD, HEADERLEN, HEADERLEN + size);
	memcpy(&(data_p->data), data, size);
	
    return data_p;
}


/**
 * create_ack -
 * Creates an acknowledgement packet.
 *
 * @param acknum    acknowledgement number
 * @return acknowledgement packet
 */
ack_packet_t * create_ack(int acknum)
{
	ack_packet_t *ack = (ack_packet_t *)malloc(sizeof(ack_packet_t));
	assert(ack != NULL);

    set_header(&(ack->header), TYPE_ACK, PAD, acknum, HEADERLEN, sizeof(ack_packet_t));
	
    return ack;
}


/**
 * create_denied -
 * Creates a denial packet.
 *
 * @return denial packet
 */
denied_packet_t * create_denied()
{
	denied_packet_t *denied = (denied_packet_t *)malloc(sizeof(denied_packet_t));
	assert(denied != NULL);
    
    set_header(&(denied->header), TYPE_DENIED, PAD, PAD, HEADERLEN, sizeof(denied_packet_t));
	
    return denied;
}


/**
 * packet_handler -
 * A general purpose packet handler that determines type of packet and deals
 * with it.
 *
 * @param sock      socketer this peer is listening on
 * @param peer      peer that packet is from
 * @param packet    packet received
 */
void packet_handler(int sock, void *peer, void *packet)
{

	header_t header;
	get_header(&header,packet); 
    
    if (header.packet_type == TYPE_WHOHAS)
    {
		whohas_handler(sock, (bt_peer_t *)peer, (whohas_packet_t *)packet);
	}
    else if(header.packet_type == TYPE_IHAVE)
	{
		ihave_handler(sock, (bt_peer_t *)peer, (ihave_packet_t *)packet);
	}
    else if(header.packet_type == TYPE_GET)
	{		
		get_handler(sock, (bt_peer_t *)peer, (get_packet_t *)packet);
	}
    else if (header.packet_type == TYPE_DATA)
    {
		data_handler(sock, (bt_peer_t *)peer, (data_packet_t *)packet);
	}
    else if (header.packet_type == TYPE_ACK)
	{
		ack_handler(sock, (bt_peer_t *)peer, (ack_packet_t *)packet);
	}
    else 
	{
		denied_handler(sock, (bt_peer_t *)peer, (denied_packet_t *)packet);
	}
}


/**
 * whohas_handler -
 * Analyzes received whohas packets and sends an/a ihave/denial packet based on
 * what chunks this peer has.
 *
 * @param sock      socket this peer is listening on
 * @param peer      peer whohas is from
 * @param whohas    whohas packet received
 */
void whohas_handler(int sock, bt_peer_t *peer, whohas_packet_t *whohas)
{
	int i, j;
	ihave_packet_t *ihave;
    uint32_t chunk_hits[HASHLEN_PACKET*(whohas->num_chunks)];
    int num_chunk_hits;
    hehas_t *next_local_chunk;
    uint32_t *next_local_chunk_hash;
    uint32_t *hashes = whohas->chunks;

	num_chunk_hits = 0;
	
    next_local_chunk = local_chunks.hehas;

    // Go through the chunks requested in the whohas packet and determine which
    // chunks are available locally.
	for (i = 0; i < whohas->num_chunks; i++)
	{
        next_local_chunk = local_chunks.hehas;
        next_local_chunk_hash = next_local_chunk->chunkhash;
        
        while(next_local_chunk != NULL)
        {
            for (j = 0; j < 5; j++)
            {
                if (hashes[i*5 + j] != next_local_chunk_hash[j])
                {
                    break;
                }

                if (j == 4)
                {
                    chunk_hits[num_chunk_hits*5] = hashes[i*5];
                    chunk_hits[num_chunk_hits*5 + 1] = hashes[i*5 + 1];
                    chunk_hits[num_chunk_hits*5 + 2] = hashes[i*5 + 2];
                    chunk_hits[num_chunk_hits*5 + 3] = hashes[i*5 + 3];
                    chunk_hits[num_chunk_hits*5 + 4] = hashes[i*5 + 4];
                    num_chunk_hits++;
                }
            }

            next_local_chunk = next_local_chunk->next;
            next_local_chunk_hash = next_local_chunk->chunkhash;            
        }
	}			

	ihave = create_ihave(chunk_hits, num_chunk_hits);	
	spiffy_sendto(sock, (void *)ihave, ihave->header.packet_len, 0, (struct sockaddr *)(&peer->addr), sizeof(peer->addr));

    peer->pending_ihave = ihave;
}


/**
 * ihave_handler -
 * Analyzes received ihave packet and sends get packets to receive packets that
 * this peer has not received yet or is not receiving.
 *
 * @param sock  socket this peer is listening on
 * @param peer  peer that ihave is from
 * @param ihave ihave packet
 */
void ihave_handler(int sock, bt_peer_t *peer, ihave_packet_t *ihave)
{
    hehas_t *next_chunk, *append_to = peer->hehas;
    int i, j, num_chunks = ihave->num_chunks;
    whohas_entry_t *tmp_entry;
    whohas_packet_t *tmp_packet;

	peer->chunks_fetched = 0;

    // When an ihave is received, delete the whohas packet that was sent from
    // the whohas queue.
    if (peer->pending_whohas != NULL)
    {
        tmp_packet = peer->whohas_list.head->whohas;
        tmp_entry = peer->whohas_list.head;
        peer->whohas_list.head = peer->whohas_list.head->next;
        
        if (peer->whohas_list.head == NULL)
        {
            peer->whohas_list.tail = NULL;
        }

        free(tmp_packet);
        free(tmp_entry);

        peer->pending_whohas = NULL;
    }

    if (num_chunks == 0)
    {    
        send_next_whohas(sock, peer);
        return;
    }

    // Depending on the ihave packet received, build a list of chunks that the
    // peer has.
    for (i = 0; i < num_chunks; i++)
    {
        next_chunk = (hehas_t *)malloc(sizeof(hehas_t));
        
        for (j = 0; j < 5; j++)
        {
            (next_chunk->chunkhash)[j] = ihave->chunks[i*5 + j];
        }
        
        next_chunk->chunk_id = get_hash_id_request(next_chunk->chunkhash);
        next_chunk->next = NULL;

        if (append_to == NULL)
        {
            peer->hehas = next_chunk;
            append_to = next_chunk;
        }
        else
        {
            append_to->next = next_chunk;
            append_to = append_to->next;
        }
    }

    peer->num_chunks = num_chunks;
    peer->get_hash_id = peer->hehas->chunk_id;

	get_next_chunk(sock, peer);
}


/**
 * get_handler -
 * Sends the requested chunk.
 *
 * @param sock  socket this peer is listening on
 * @param peer  peer get is from
 * @param get   get packet received
 */
void get_handler(int sock, bt_peer_t *peer, get_packet_t *get)
{
    me->curr_to = peer;

    // If this peer is already serving a request, deny the new get request.
	if (me->his_request != NULL)
	{
		denied_packet_t * denied = create_denied();
		spiffy_sendto(sock,(void *)denied,denied->header.packet_len,0,(struct sockaddr *)(&peer->addr),sizeof(peer->addr));
		free(denied);
		return;	
	}

    // Once the get packet is confirmed for the ihave packet that was previously
    // send, delete the pending ihave packet.
    if (peer->pending_ihave != NULL)
    {
        free(peer->pending_ihave);
        peer->pending_ihave = NULL;
    }

    // Mark that this peer is serving a request.
    me->his_request = get;
	peer->his_request = get;

	peer->send_hash_id = get_hash_id_master(peer->his_request->hash);

    init_congestion_state(&state);
	send_next_data(sock, peer, peer->send_hash_id, 1);
}


/**
 * data_handler - 
 * Write received data to file.
 *
 * @param sock      socket this peer is listening on
 * @param peer      peer data_p is from
 * @param data_p    received data_packet
 */
void data_handler(int sock, bt_peer_t *peer, data_packet_t *data_p)
{
    FILE *fp;
    int seqnum;
    int acknum;
	int offset;
    int size;
	ack_packet_t* ack;

    peer->ack_timeout_counter = 0;

    // The get_hash_id being infinity means either no get request was made or
    // all chunks have already been retrieved. Ignore the data packet.
    if (peer->get_hash_id == PSUEDO_INF)
    {
        free(data_p);
        get_next_chunk(sock, peer);
        return;
    }

    if (peer->pending_get != NULL)
    {
        free(peer->pending_get);
        peer->pending_get = NULL;
    }
    
    // Determine offset of the received data in file to write to.
    seqnum = data_p->header.seq_num;
    offset = (peer->get_hash_id)*CHUNKLEN + (seqnum - 1)*BUFLEN;

    size = data_p->header.packet_len - data_p->header.header_len;

    fp = fopen(config.output_file, "rb+");

    if (fp == NULL)
    {
        fp = fopen(config.output_file, "wb");
    }
    
    rewind(fp);

    if (offset != 0)
    {
        fseek(fp, offset, SEEK_SET);
    }
    else
    {
    }

    fwrite(data_p->data, 1, size, fp);
    fclose(fp);

    // Insert the data packet into the corresponding spot in the ack sliding
    // window and send out an ack.
    insert_entry(&(peer->recv_buffer), time(NULL), data_p);
    acknum = find_ack(&(peer->recv_buffer));
    ack = create_ack(acknum);
    spiffy_sendto(sock, (void *)ack, ack->header.packet_len, 0, (struct sockaddr *)&(peer->addr), sizeof(peer->addr));
    free(ack);

    // If the data packet is the final data packet for requested chunk,
    // initialize state for next chunk retrieval.
    if (acknum == FINALACK)
    {
        post_receive_cleanup(peer);

        if (peer->chunks_fetched < peer->num_chunks)
        {
            get_next_chunk(sock, peer);
        }
        // All chunks have been retrieved, send out the next whohas.
        else
        {
            peer->num_chunks = 0;
            peer->chunks_fetched = 0;
            free(peer->hehas);
            peer->hehas = NULL;
            //destroy_fof(me);
            send_next_whohas(sock, peer);
        }

        if (me->num_chunks == me->chunks_fetched)
        {
            printf("DOWNLAOD COMPLETE\n");
            destroy_fof(me);
            me->num_chunks = 0;
            me->chunks_fetched = 0;
        }
    }
}


/**
 * ack_handler - 
 * Modeified slididing window based on acknowledgement packet recvied and sends
 * out the next packet.
 *
 * @param sock  socket this peer is listening on
 * @param peer  peer that ack is from
 * @param ack   acknowledgement packet
 */
void ack_handler(int sock, bt_peer_t *peer, ack_packet_t *ack)
{
    peer->num_data_retransmits = 0;

    // Ignore ack if it is greater than the sequence number of last data packet
    // sent.
    if (ack->header.ack_num > send_buffer.tail->data_packet->header.seq_num)
    {
        free(ack);
        return;
    }

    // If an ack was received and not request was actually made, ignore ack.
    if (peer->his_request == NULL)
    {
        free(ack);
        return;
    }

    // If ack is equal to last ack received, increment the number of dupacks.
    if (peer->last_ack == ack->header.ack_num)
    {
        peer->num_dupacks++;
        destroy_upto_ack(&send_buffer, peer->last_ack);
    }
    // If ack is smaller than the last ack received, ignore.
    else if (peer->last_ack > ack->header.ack_num)
    {
        destroy_upto_ack(&send_buffer, peer->last_ack);
    }
    // If ack is greater than tha last ack received, destory packets in the send
    // sliding window, assuming that all packets before the ack were delived.
    else
    {
        peer->last_ack = ack->header.ack_num;
        peer->num_dupacks = 1;
        destroy_upto_ack(&send_buffer, peer->last_ack);
    }

    // If ack is the ack from final data packet, sending chuck is finished.
    // Initialize state for sending next chunk.
    if (me->last_seq == FINALACK && peer->last_ack == FINALACK)
    {
        me->curr_to = NULL;
        post_send_cleanup(peer);
        return;
    }

    // In the case of a triple dupack, retransmit the packet.
    if (peer->num_dupacks == 3)
    {
        peer->num_dupacks = 1;

        mult_decrease(&state);
        retransmit_data(sock, peer);
    }
    // Else, do regular transmission of next data packet.
    else
    {
        slow_start(&state);
        send_next_data(sock, peer, peer->send_hash_id, me->last_seq + 1);
    }
}


/**
 * denied_handler -
 * Stop talking to the peer that the denied packet is from.
 *
 * @param sock      socket this peer is listening on
 * @param peer      peer that denied is from
 * @param denied    denied packet
 */
void denied_handler(int sock, bt_peer_t *peer, denied_packet_t * denied)
{
    whohas_entry_t *prev;
    whohas_entry_t *curr;

    fprintf(stderr, "Request denial.\n");
    
    // In the case of a denial, assume that this request cannot be fulfilled.
    // Initialize all states.

    if (peer->pending_get != NULL)
    {
        free(peer->pending_get);
        peer->pending_get = NULL;
    }

    if (peer->pending_ihave != NULL)
    {
        free(peer->pending_ihave);
        peer->pending_ihave = NULL;
    }

    if (peer->pending_whohas != NULL)
    {
        curr = peer->whohas_list.head;
        while (curr != NULL)
        {
            prev = curr;
            curr = curr->next;

            free(prev->whohas);
            free(prev);
        }

        peer->whohas_list.head = NULL;
        peer->whohas_list.tail = NULL;

        peer->pending_whohas = NULL;
    }

    free(denied);
}


/**
 * init_whohas -
 * Upon user request, initialized the whohas packet based on the chunks
 * requested.
 *
 * @param peers         peers to send the whohas packets to
 * @param num_chunks    number of chunks requested
 * @param hashes        hash values of chunks requested
 * @param my_id         identity number of this peer
 */
void init_whohas(bt_peer_t *peers, int num_chunks, char *hashes, int my_id)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    char *temp_hashes = hashes;
    int iterations = ceil((float)num_chunks/(float)MAX_CHUNKS_PER_PACK);
    int send_count, i;
    whohas_packet_t *whohas = NULL;
    whohas_entry_t *whohas_wrapper = NULL;
    bt_peer_t *peer = (bt_peer_t *)peers;

    // Go through the chunk hashes and divide them into separate whohas packet
    // if not all of the can fit in a single packet.
    for (i = 0; i < iterations; i++)
    {
        if(iterations == 1)
		{
			send_count = num_chunks;
		}
		else if(iterations != 1 && i == iterations - 1)
		{
			send_count = num_chunks - i*MAX_CHUNKS_PER_PACK;
		}
		else
		{
			send_count = MAX_CHUNKS_PER_PACK;
        }

        temp_hashes = hashes + i*MAX_CHUNKS_PER_PACK*HASHLEN;

        peer = (bt_peer_t *)peers;
        while (peer!= NULL)
        {
            whohas = create_whohas(send_count, temp_hashes);
            whohas_wrapper = (whohas_entry_t *)malloc(sizeof(whohas_entry_t));
            whohas_wrapper->whohas = whohas;
            whohas_wrapper->next = NULL;

            if (peer->id != my_id)
            {
                if (peer->whohas_list.head == NULL)
                {
                    peer->whohas_list.head = whohas_wrapper;
                    peer->whohas_list.tail = whohas_wrapper;
                }
                else
                {
                    peer->whohas_list.tail->next = whohas_wrapper;
                    peer->whohas_list.tail = whohas_wrapper;
                }
            }

            peer = peer->next;
        }
    }

    peer = (bt_peer_t *)peers;

    me->num_chunks = num_chunks;
    me->chunks_fetched = 0;
    
    // Send the next whohas packet to all peers.
    while (peer != NULL)
    {
        if (peer->id == my_id)
        {
            peer = peer->next;
            continue;
        }

        send_next_whohas(sock, peer);
        
        peer = peer->next;
    }
}


/**
 * send_next_whohas -
 * Sends the next whohas packet in queue.
 *
 * @param sock  socket this peer is listening on
 * @param peer  peer to send whohas packet to
 */
void send_next_whohas(int sock, bt_peer_t *peer)
{
    whohas_entry_t *whohas_entry;
    whohas_packet_t *whohas;

    if (peer->whohas_list.head == NULL)
    {
        return;
    }

    whohas_entry = peer->whohas_list.head;
    whohas = whohas_entry->whohas;

    spiffy_sendto(sock,(void *)whohas,whohas->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
    
    peer->pending_whohas = peer->whohas_list.head->whohas;
}


/**
 * get_next_chunk -
 * Sends ou the next get packet in queue.
 *
 * @param sock  socket this peer is listening on
 * @param peer  peer to send get packet to
 */
void get_next_chunk(int sock, bt_peer_t *peer)
{
	get_packet_t *get_p;
    hehas_t *next_chunk = peer->hehas;
	fetching_or_fetched_t *prev_fof, *fof = me->fetching_or_fetched;

    // If all chunks have been received, don't get any more chunks.
    if (peer->num_chunks == peer->chunks_fetched)
    {
        return;
    }

    // If the next chunk to get is null, there are no more chunks to receive.
    // Dont get any more chunks.
    if (next_chunk == NULL)
    {
        return;
    }

    // Inspect the list of chunks that are already fetched or are being fetched.
    // Get the chunks that are not in the list.
    if (fof == NULL)
    {
        me->fetching_or_fetched = (fetching_or_fetched_t *)malloc(sizeof(fetching_or_fetched_t));
        me->fetching_or_fetched->chunk_id = next_chunk->chunk_id;
        me->fetching_or_fetched->next = NULL;
    }
    else
    {
        while (next_chunk != NULL)
        {
            fof = me->fetching_or_fetched;
            while(fof != NULL)
            {
                if (next_chunk->chunk_id == fof->chunk_id)
                {
                    peer->hehas = next_chunk->next;
                    free(next_chunk);
                    next_chunk = peer->hehas;
                    peer->chunks_fetched++;
                    break;
                }
                prev_fof = fof;
                fof = fof->next;
            }

            if (fof == NULL)
            {    
                prev_fof->next = (fetching_or_fetched_t *)malloc(sizeof(fetching_or_fetched_t));
                prev_fof->next->chunk_id = next_chunk->chunk_id;
                prev_fof->next->next = NULL;
                break;
            }
        }
    }

    // In this case, all chunks are already being fetched or have been fetched.
    // Don't send a request.
    if (next_chunk == NULL)
    {
        return;
    }

    peer->get_hash_id = next_chunk->chunk_id;
    get_p = create_get(next_chunk->chunkhash);

    peer->pending_get = get_p;

	spiffy_sendto(sock, (void *)get_p, get_p->header.packet_len, 0, (struct sockaddr *)&(peer->addr), sizeof(peer->addr));
}


/**
 * send_next_data -
 * Send next data in queue.
 *
 * @param sock      socket this peer is listening on
 * @param peer      peer to send data to
 * @param hash_id   hash id of the chunk that data is in
 * @param seqnum    sequence number of this transmission
 */
void send_next_data(int sock, bt_peer_t *peer, int hash_id, int seqnum)
{
    int i;
    int tmpseqnum;
    FILE *fp;
	data_packet_t *data_p;
	char data[BUFLEN];
    int bytes_to_read;
	int bytes_read;
    int offset;

    // Congestion control. The window is already full, don't send anything
    // this time.
    if (seqnum > peer->last_ack + state.cwnd)
    {
        return;
    }

    offset = (CHUNKLEN*hash_id) + ((seqnum-1)*BUFLEN);

    fp = fopen(master_data_file, "rb");
    rewind(fp);
    
    for (i = 0; i < hash_id; i++)
    {
        fseek(fp, CHUNKLEN, SEEK_CUR);
    }

    for (i = 0; i < seqnum-1; i++)
    {
        fseek(fp, BUFLEN, SEEK_CUR);
    }

    tmpseqnum = seqnum;

    // Send packets as long as there a spaces available in the window.
    bytes_to_read = 0;
    bytes_read = 0;
    while (offset < hash_id*CHUNKLEN + CHUNKLEN && tmpseqnum <= peer->last_ack + state.cwnd)
    {
        if (send_buffer.num_entry == state.cwnd)
        {
            break;
        }
        
        if (hash_id*CHUNKLEN + CHUNKLEN - offset < BUFLEN)
        {
            bytes_to_read = hash_id*CHUNKLEN + CHUNKLEN - offset;
        }
        else
        {
            bytes_to_read = BUFLEN;
        }

        memset(data, 0, BUFLEN);
        bytes_read = fread(data, 1, bytes_to_read, fp);

        me->last_seq = tmpseqnum;
        data_p = create_data(data, tmpseqnum++, bytes_read);
        append_entry(&send_buffer, time(NULL), data_p);
        spiffy_sendto(sock, (void *)data_p, data_p->header.packet_len, 0, (struct sockaddr *)&(peer->addr), sizeof(peer->addr));
        
        state.curr_round++;

        offset += bytes_read;
    }

    fclose(fp);

    add_increase(&state);
}


/**
 * retransmit_data - 
 * Function for data retransmission.
 *
 * @param sock  socket this peer is listening on
 * @param peer  peer to retransmit to
 */
void retransmit_data(int sock, bt_peer_t *peer)
{
    spiffy_sendto(sock, send_buffer.head->data_packet, send_buffer.head->data_packet->header.packet_len, 0, (struct sockaddr *)&(peer->addr),sizeof(peer->addr));
}


/**
 * retransmit_whohas -
 * Function to retransmit whohas packets.
 *
 * @param sock  socket this peer is listening on
 * @param peer  peer to retransmit whohas packet to
 */
void retransmit_whohas(int sock, bt_peer_t *peer)
{
    spiffy_sendto(sock, peer->pending_whohas, peer->pending_whohas->header.packet_len, 0, (struct sockaddr *)&(peer->addr),sizeof(peer->addr));
}


/**
 * retransmit_ihave -
 * Function to retransmit ihave packets.
 *
 * @param sock  socket this peer is listening on
 * @param peer  peer to retransmit ihave packet to
 */
void retransmit_ihave(int sock, bt_peer_t *peer)
{
    spiffy_sendto(sock, peer->pending_ihave, peer->pending_ihave->header.packet_len, 0, (struct sockaddr *)&(peer->addr),sizeof(peer->addr));
}


/**
 * retransmit_get -
 * Function to retransmit get packets.
 *
 * @param sock  socket this peer is listening on
 * @param peer  peer to retransmit get packet to
 */
void retransmit_get(int sock, bt_peer_t *peer)
{
    spiffy_sendto(sock, peer->pending_get, peer->pending_get->header.packet_len, 0, (struct sockaddr *)&(peer->addr),sizeof(peer->addr));
}


/**
 * get_hash_id_master -
 * Determines the hash id of chunk has in the master hash file
 *
 * @param hash  chunk hash
 * @return hash id, -1 if no match is found
 */
int get_hash_id_master(uint32_t *hash)
{
    int i;
    hehas_t *next_chunk = master_chunks.hehas;

    while (next_chunk != NULL)
    {
        for (i = 0; i < 5; i++)
        {
            if (hash[i] != (next_chunk->chunkhash)[i])
            {
                break;
            }

            if (i == 4)
            {
                return next_chunk->chunk_id;
            }
        }

        next_chunk = next_chunk->next;
    }

    return -1;
}


/**
 * get_hash_id_request -
 * Determines the hash id of requested chunks in the corresponding haschunks
 * file.
 *
 * @param hash  chunk hash
 * @return hash id, -1 if no match is found
 */
int get_hash_id_request(uint32_t *hash)
{
    int i;
    int id;
    char *my_hash;
    char buf[BUFLEN];
    
    FILE *fp = fopen(request_chunks_file, "r");
    
    while (fgets(buf, BUFLEN, fp) != NULL)
    {
        id = atoi(strtok(buf, " "));
        my_hash = strtok(NULL, " ");
        my_hash[HASHLEN] = '\0';
        
        for (i = 0; i < 5; i++)
        {
            if (int_str_comp(hash[i], my_hash, i*8, 8) != 0)
            {
                break;
            }

            if (i == 4)
            {
                return id;
            }
        }

        buf[0] = '\0';
    }

    return -1;
}


/**
 * post_send_cleanup - 
 * Cleans up connection after finishing sending a chunk.
 *
 * @param peer  peer that send was performed on
 */
void post_send_cleanup(bt_peer_t *peer)
{
    peer->last_ack = 0;
    peer->num_dupacks = 0;
    peer->his_request = NULL;
    me->his_request = NULL;
    peer->send_hash_id = PSUEDO_INF;
    destroy_entries(&send_buffer);
}


/**
 * post_receive_cleanup -
 * Cleans up connection after finishing receiving a chunk.
 *
 * @param peer  peer chunk was being received from
 */
void post_receive_cleanup(bt_peer_t *peer)
{
    hehas_t *tmphehas = peer->hehas;
    if (tmphehas != NULL)
    {
        peer->hehas = peer->hehas->next;
        free(tmphehas);
    }
    peer->chunks_fetched++;
    me->chunks_fetched++;
    peer->get_hash_id = PSUEDO_INF;
    destroy_entries(&(peer->recv_buffer));
}


/**
 * destroy_fof -
 * Cleans up the currently being received or received list.
 *
 * @param peer  peer that fetching or fetched list is with
 */
void destroy_fof(bt_peer_t *peer)
{
    fetching_or_fetched_t *prev;
    fetching_or_fetched_t *curr;

    if (peer == NULL)
    {
        return;
    }

    curr = peer->fetching_or_fetched;

    while(curr != NULL)
    {
        prev = curr;
        curr = curr->next;
        free(prev);
    }

    peer->fetching_or_fetched = NULL;
}


/**
 * destroy_hehas -
 * Collpases list of chunks a peer has.
 *
 * @param peer   peer that has the list
 */
void destroy_hehas(bt_peer_t *peer)
{
    hehas_t *prev;
    hehas_t *curr = peer->hehas;

    while (curr != NULL)
    {
        prev = curr;
        curr = curr->next;

        free(prev);
    }

    peer->hehas = NULL;
}


/**
 * destroy_whohas_list -
 * Collapses queue of whohas packets to send.
 *
 * @param peer  peer that has the whohas list
 */
void destroy_whohas_list(bt_peer_t *peer)
{
    whohas_entry_t *prev;
    whohas_entry_t *curr = peer->whohas_list.head;

    while (curr != NULL)
    {
        prev = curr;
        curr = curr->next;

        if (prev->whohas != NULL)
        {
            free(prev->whohas);
        }

        free(prev);
    }

    peer->whohas_list.head = NULL;
    peer->whohas_list.tail = NULL;
}
