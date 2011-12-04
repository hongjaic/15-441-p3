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

#include "packets.h"
#include "packet_handler.h"
#include "bt_parse.h"
#include "spiffy.h"


int get_hash_id(char *hash);
int get_hash_id_master(uint32_t *hash);
int get_hash_id_test(uint32_t *hash);

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



void *bytes_to_packet(char *buf, int size)
{
	void *packet = (void *)malloc(size);
	memcpy(packet,buf,size);	
	return packet;
}


//returns the header of the packet
void get_header(header_t *header, void *packet)
{
	memcpy(header, packet, HEADERLEN);
}


//sets the packet header
void set_header(header_t *header,int type, int seq_num, int ack_num, int header_len, int packet_len){

	header->magicnum = MAGIC;
	header->version = VERSION;
	header->packet_type = type;
	header->header_len = header_len;
	header->packet_len = packet_len;
	header->seq_num = seq_num;
	header->ack_num = ack_num;	
}


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


ihave_packet_t * create_ihave(uint32_t *chunk_hits, int chunks_num)
{
	ihave_packet_t *ihave = (ihave_packet_t *)malloc(HEADERLEN + 4 + chunks_num*HASHLEN_PACKET);
	assert(ihave != NULL);
    
    set_header(&(ihave->header), TYPE_IHAVE, PAD, PAD, HEADERLEN, HEADERLEN + 4 + chunks_num*HASHLEN_PACKET);
	ihave->num_chunks = chunks_num;
    memcpy(&(ihave->chunks), chunk_hits, HASHLEN_PACKET*chunks_num);

	return ihave;
}


get_packet_t * create_get(uint32_t *hash)
{
	get_packet_t *get = (get_packet_t *)malloc(HEADERLEN + HASHLEN_PACKET);
	assert(get != NULL);
    
    set_header(&(get->header), TYPE_GET, PAD, PAD, HEADERLEN, HEADERLEN + HASHLEN_PACKET);
	memcpy(&(get->hash), hash, HASHLEN_PACKET);
	
    return get;
}


data_packet_t * create_data(char *data, int seqnum, int size)
{
	data_packet_t *data_p = (data_packet_t *)malloc(HEADERLEN + size);
	assert(data_p != NULL);
    
    set_header(&(data_p->header), TYPE_DATA, seqnum, PAD, HEADERLEN, HEADERLEN + size);
	memcpy(&(data_p->data), data, size);
	
    return data_p;
}


ack_packet_t * create_ack(int acknum)
{
	ack_packet_t *ack = (ack_packet_t *)malloc(sizeof(ack_packet_t));
	assert(ack != NULL);

    set_header(&(ack->header), TYPE_ACK, PAD, acknum, HEADERLEN, sizeof(ack_packet_t));
	
    return ack;
}

denied_packet_t * create_denied()
{
	denied_packet_t *denied = (denied_packet_t *)malloc(sizeof(denied_packet_t));
	assert(denied != NULL);
    
    set_header(&(denied->header), TYPE_DENIED, PAD, PAD, HEADERLEN, sizeof(denied_packet_t));
	
    return denied;
}


/*
This is the genreic handler that gets called whenever we receive a packet.
We do not know the packet type, so here we determine that, and call the appropriate
handler
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


/*
Call whenever this peer receives a whohas packet.
The peer compares all the packets in the whohas payload against
his local chunks and creates a new struct with all the
common chunks. We then create an ihave packet with and return it to the 
source 
*/

void whohas_handler(int sock, bt_peer_t *peer, whohas_packet_t *whohas)
{
    printf("whohas handler init\n");

	int i, j;
	ihave_packet_t *ihave;
    denied_packet_t * denied;
    uint32_t chunk_hits[HASHLEN_PACKET*(whohas->num_chunks)];
    int num_chunk_hits;
    hehas_t *next_local_chunk;
    uint32_t *next_local_chunk_hash;
    uint32_t *hashes = whohas->chunks;

	num_chunk_hits = 0;
	
    next_local_chunk = local_chunks.hehas;

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

	if(num_chunk_hits != 0)
    {
	    ihave = create_ihave(chunk_hits, num_chunk_hits);	
		//printf("sending %d chunks\n",ihave->chunks.num_chunks);
		spiffy_sendto(sock, (void *)ihave, ihave->header.packet_len, 0, (struct sockaddr *)(&peer->addr), sizeof(peer->addr));
		//free(ihave);

        peer->pending_ihave = ihave;
	}
    else
    {
        denied = create_denied();
		spiffy_sendto(sock,(void *)denied,denied->header.packet_len,0,(struct sockaddr *)(&peer->addr),sizeof(peer->addr));
		free(denied);
    }

    printf("whowhas handler fin\n");
}


/*
Gets called whenever this peer receives an "ihave" packet.
We send a "Get" request for the first chunk in the packet.
This will trigger a chain of events until all the chunks in this ihave packet
have been downloaded.(Currently, we "naively" ask that peer for all of the chunks 
in the "ihave" packet... we want to only ask him for some of them.. to do )

*/

void ihave_handler(int sock, bt_peer_t *peer, ihave_packet_t *ihave)
{
    printf("ihave_handler init\n");

    hehas_t *next_chunk, *append_to = peer->hehas;
    int i, j, num_chunks = ihave->num_chunks;
    whohas_entry_t *tmp_entry;
    whohas_packet_t *tmp_packet;
	//printf("node %d has %d chunks:\n",peer->id, ihave->chunks.num_chunks);
	//peer->hehas = hehas;
	//peer->bytes_received = 0;
	peer->chunks_fetched = 0;
    
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

    for (i = 0; i < num_chunks; i++)
    {
        next_chunk = (hehas_t *)malloc(sizeof(hehas_t));
        
        for (j = 0; j < 5; j++)
        {
            (next_chunk->chunkhash)[j] = ihave->chunks[i*5 + j];
        }
        
        next_chunk->chunk_id = get_hash_id_test(next_chunk->chunkhash);
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

    printf("ihave_handler fin\n");
}


/*
Called whenever we recevie a "Get" packet. We check if we are currently 
servicing another get request, if so, send a denial packet.
If we are free, we send the first piece of chunk data with sequence number 1
*/
void get_handler(int sock, bt_peer_t *peer, get_packet_t *get)
{
    printf("get_handler init\n");

    me->curr_to = peer;

	if (me->his_request != NULL)
	{
		printf("!!!!!DENIALLLL!!!!!\n");
		denied_packet_t * denied = create_denied();
		spiffy_sendto(sock,(void *)denied,denied->header.packet_len,0,(struct sockaddr *)(&peer->addr),sizeof(peer->addr));
		free(denied);
		return;	
	}

    if (peer->pending_ihave != NULL)
    {
        free(peer->pending_ihave);
        peer->pending_ihave = NULL;
    }

    me->his_request = get;
	peer->his_request = get;

	peer->send_hash_id = get_hash_id_master(peer->his_request->hash);


    /* do something about seqnum */
    init_congestion_state(&state);
	send_next_data(sock, peer, peer->send_hash_id, 1);

    printf("get_handler fin\n");
}


/*
Called whenever we receive a "data" packet. we write the data to the output file
we then create and send an ACK. (which will trigger another data packet to be sent.

If we receive the last data packet of a chunk, we create a new "get" request for the next chunk that the peer advertised in his "i have" packet...
*/
void data_handler(int sock, bt_peer_t *peer, data_packet_t *data_p)
{
    printf("data_handler init\n");

    printf("type: %d\n", data_p->header.packet_type);
    printf("magicnum: %d\n", data_p->header.magicnum);
    printf("seqnum: %d\n", data_p->header.seq_num);

    FILE *fp;
    int seqnum;
    int acknum;
	int offset;
    int size;
	ack_packet_t* ack;

    if (peer->get_hash_id == PSUEDO_INF)
    {
        printf("what the fuck psuedo_inf\n");
        free(data_p);
        get_next_chunk(sock, peer);
        return;
    }

    if (peer->pending_get != NULL)
    {
        free(peer->pending_get);
        peer->pending_get = NULL;
    }
    
    /** rethink if this is correct **/
    seqnum = data_p->header.seq_num;
    offset = (peer->get_hash_id)*CHUNKLEN + (seqnum - 1)*BUFLEN;

    printf("type: %d\n", data_p->header.packet_type);
    printf("magicnum: %d\n", data_p->header.magicnum);
    printf("offset: %d\n", offset);
    printf("seqnum: %d\n", data_p->header.seq_num);

    /** rethink if this is correct **/
    size = data_p->header.packet_len - data_p->header.header_len;

    fp = fopen(config.output_file, "rb+");

    if (fp == NULL)
    {
        fp = fopen(config.output_file, "wb");
    }
    
    rewind(fp);;

    if (offset != 0)
    {
        fseek(fp, offset, SEEK_SET);
    }
    else
    {
        printf("offset = 0\n");
    }

    fwrite(data_p->data, 1, size, fp);
    fclose(fp);

    insert_entry(&recv_buffer, time(NULL), data_p);
    
    acknum = find_ack(&recv_buffer);

    printf("acknum: %d\n", acknum);

    ack = create_ack(acknum);


    spiffy_sendto(sock, (void *)ack, ack->header.packet_len, 0, (struct sockaddr *)&(peer->addr), sizeof(peer->addr));

    free(ack);



    //printf("acknum: %d\n", acknum);
    //printf("hashid: %d\n", peer->get_hash_id);



    if (acknum == FINALACK)
    {
        post_receive_cleanup(peer);

        if (peer->chunks_fetched < peer->num_chunks)
        {
            get_next_chunk(sock, peer);
        }
        else
        {
            peer->num_chunks = 0;
            peer->chunks_fetched = 0;
            free(peer->hehas);
            peer->hehas = NULL;
            destroy_fof(me);
            printf("FUCK\n");

            send_next_whohas(sock, peer);
        }
    }

    printf("data_handler fin\n");
}


/*
Called whenever we receive an "ack" packet. we verify that it is the ack we are expecting from that peer. and if so, we send the next piece of data for the chunk requested until we've sent the whole chunk
*/

void ack_handler(int sock, bt_peer_t *peer, ack_packet_t *ack)
{
    printf("ack_handler init\n");

    if (ack->header.ack_num > send_buffer.tail->data_packet->header.seq_num)
    {
        free(ack);
        return;
    }

    if (peer->his_request == NULL)
    {
        free(ack);
        return;
    }

    if (peer->last_ack == ack->header.ack_num)
    {
        peer->num_dupacks++;
        destroy_upto_ack(&send_buffer, peer->last_ack);
    }
    else if (peer->last_ack > ack->header.ack_num)
    {
        // do nothing
        
        destroy_upto_ack(&send_buffer, peer->last_ack);
    }
    else
    {
        peer->last_ack = ack->header.ack_num;
        peer->num_dupacks = 1;
        destroy_upto_ack(&send_buffer, peer->last_ack);
    }

    if (me->last_seq == FINALACK && peer->last_ack == FINALACK)
    {
        printf("ack_handler fin due to FINALACK\n");
        me->curr_to = NULL;
        post_send_cleanup(peer);
        return;
    }

    printf("last_ack: %d\n", peer->last_ack);

    if (peer->num_dupacks == 3)
    {
        //send_next_data(sock, peer, peer->send_hash_id, peer->last_ack + 1);
        peer->num_dupacks = 1;

        mult_decrease(&state);
        printf("triple dupacks, regen seq num is: %d\n", send_buffer.head->data_packet->header.seq_num);
		//spiffy_sendto(sock, send_buffer.head->data_packet, send_buffer.head->data_packet->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
        retransmit_data(sock, peer);
    }
    else
    {
        slow_start(&state);
        send_next_data(sock, peer, peer->send_hash_id, me->last_seq + 1);//peer->last_seq + 1);// + peer->num_dupacks);
    }

    printf("ack_handler fin\n");
}


void denied_handler(int sock,bt_peer_t *peer, denied_packet_t * denied)
{
    whohas_entry_t *prev;
    whohas_entry_t *curr;

	printf("!!!GOT DENIED!!!\n");
    
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
}


void init_whohas(bt_peer_t *peers, int num_chunks, char *hashes, int my_id)
{
    printf("send whohas init\n");

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    char *temp_hashes = hashes;
    int iterations = ceil((float)num_chunks/(float)MAX_CHUNKS_PER_PACK);
    int send_count, i;
    whohas_packet_t *whohas = NULL;
    whohas_entry_t *whohas_wrapper = NULL;
    bt_peer_t *peer = (bt_peer_t *)peers;

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
    
    while (peer != NULL)
    {
        if (peer->id == my_id)
        {
            peer = peer->next;
            continue;
        }

        send_next_whohas(sock, peer);
        
        peer = peer->next;
    };
    
    printf("send whohas fin\n");
}

void send_next_whohas(int sock, bt_peer_t *peer)
{
    printf("+++++send_next_whohas init+++++\n");

    whohas_entry_t *whohas_entry;
    whohas_packet_t *whohas;

    if (peer->whohas_list.head == NULL)
    {
        printf("No more WHOHAS LEFT TO SEND TO peer: %d\n", peer->id);
        return;
    }

    whohas_entry = peer->whohas_list.head;
    whohas = whohas_entry->whohas;

    spiffy_sendto(sock,(void *)whohas,whohas->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
    
    //peer->whohas_list.head = whohas_entry->next;
    //if (peer->whohas_list.head == NULL)
    //{
    //    peer->whohas_list.tail = NULL;
    //}

    //free(whohas);
    //free(whohas_entry);
    
    peer->pending_whohas = peer->whohas_list.head->whohas;

    printf("+++++send_next_whohas fin+++++\n");
}


/*
creates a new "get" packet and asks for the next chunk from peer. 
*/
void get_next_chunk(int sock, bt_peer_t *peer)
{
    printf("get_next_chunk init\n");

	get_packet_t *get_p;
    hehas_t *next_chunk = peer->hehas;
	fetching_or_fetched_t *prev_fof, *fof = me->fetching_or_fetched;

    if (peer->num_chunks == peer->chunks_fetched)
    {
        printf("get_next_chunk fin due to no more chunks to get\n");
        return;
    }

    if (next_chunk == NULL)
    {
        printf("get_next_chunk fin due to next chunk null\n");
        return;
    }

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

    if (next_chunk == NULL)
    {
        printf("get_next_chunk fin due to next chunk null2\n");
        return;
    }

    peer->get_hash_id = next_chunk->chunk_id;
    get_p = create_get(next_chunk->chunkhash);

    peer->pending_get = get_p;

	spiffy_sendto(sock, (void *)get_p, get_p->header.packet_len, 0, (struct sockaddr *)&(peer->addr), sizeof(peer->addr));

    //free(get_p);

    printf("get_next_chunk fin\n");
}


/*
This sends the next piece of chunk data from chunk with id = "hash_id" to the peer.
*/
void send_next_data(int sock, bt_peer_t *peer, int hash_id, int seqnum)
{
    printf("==============send_next_data init===================\n");

	//int seqnum;
    int errnoSave;
    int i;
    int tmpseqnum;
    FILE *fp;
	data_packet_t *data_p;
	char data[BUFLEN];
    int bytes_to_read;
	int bytes_read;// = BUFLEN;
    int offset;

    printf("seqnum: %d\n", seqnum);
    printf("cwnd: %d\n", state.cwnd);
    printf("lastack: %d\n", peer->last_ack);

    if (seqnum > peer->last_ack + state.cwnd)// || send_buffer.num_entry == state.cwnd)// cwnd)
    {
        printf("send_next_data fin due to not enough window 1\n");
        return;
    }

    offset = (CHUNKLEN*hash_id) + ((seqnum-1)*BUFLEN);
    //printf("BUFLEN: %d\n", BUFLEN);
    //printf("offset: %d\n", offset);

    fp = fopen(master_data_file, "rb");
    rewind(fp);
    
    for (i = 0; i < hash_id; i++)
    {
        fseek(fp, CHUNKLEN, SEEK_CUR);
    }

    printf("SHIT: %d\n", (seqnum-1)*BUFLEN);

    for (i = 0; i < seqnum-1; i++)
    {
        fseek(fp, BUFLEN, SEEK_CUR);
    }

    tmpseqnum = seqnum;

    bytes_to_read = 0;
    bytes_read = 0;
    while (offset < hash_id*CHUNKLEN + CHUNKLEN && tmpseqnum <= peer->last_ack + state.cwnd)
    {
        printf("seqnum: %d\n", tmpseqnum);

        if (send_buffer.num_entry == state.cwnd)// cwnd)
        {
            printf("2\n");
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

        errnoSave = errno;

        printf("offset: %d\n", offset);
        if(bytes_read != bytes_to_read)
        {
            printf("errno: %d\n", errnoSave);
            printf("tmpseqnum: %d\n", tmpseqnum);
            printf("offset: %d\n", offset);
            printf("bytes_to_read: %d\n", bytes_to_read);
            printf("bytes_read: %d\n", bytes_read);
            printf("FUCCCCCCCCCCK!!!\n");
            exit(EXIT_FAILURE);
        }

        me->last_seq = tmpseqnum;

        data_p = create_data(data, tmpseqnum++, bytes_read);

        append_entry(&send_buffer, time(NULL), data_p);

        spiffy_sendto(sock, (void *)data_p, data_p->header.packet_len, 0, (struct sockaddr *)&(peer->addr), sizeof(peer->addr));
        
        state.curr_round++;
        //free(data_p);

        offset += bytes_read;
    }

    fclose(fp);

    add_increase(&state);
    printf("==================send_next_data fin================\n");
}


void retransmit_data(int sock, bt_peer_t *peer)
{
    printf("retransmit seqnum: %d\n", send_buffer.head->data_packet->header.seq_num);
    spiffy_sendto(sock, send_buffer.head->data_packet, send_buffer.head->data_packet->header.packet_len, 0, (struct sockaddr *)&(peer->addr),sizeof(peer->addr));
}


void retransmit_whohas(int sock, bt_peer_t *peer)
{
    printf("retransmit whohas\n");
    spiffy_sendto(sock, peer->pending_whohas, peer->pending_whohas->header.packet_len, 0, (struct sockaddr *)&(peer->addr),sizeof(peer->addr));
}


void retransmit_ihave(int sock, bt_peer_t *peer)
{
    printf("retransmit ihave\n");
    spiffy_sendto(sock, peer->pending_ihave, peer->pending_ihave->header.packet_len, 0, (struct sockaddr *)&(peer->addr),sizeof(peer->addr));
}


void retransmit_get(int sock, bt_peer_t *peer)
{
    printf("retransmit get\n");
    spiffy_sendto(sock, peer->pending_get, peer->pending_get->header.packet_len, 0, (struct sockaddr *)&(peer->addr),sizeof(peer->addr));
}


/*
given a hash value, this returns the hash ID as it pertains to the master chunks data file
*/
//int get_hash_id(char *hash)
//{
//    char tmphash[HASHLEN + 1];
//    hehas_t *next_chunk = local_chunks.hehas;
    
//    memcpy(tmphash, hash, HASHLEN);
//    tmphash[HASHLEN] = '\0';
    
//    while (next_chunk != NULL)
//    {       
//        if (strcmp(tmphash, next_chunk->chunkhash) == 0)
//        {
//            return next_chunk->chunk_id;
//        }

//        next_chunk = next_chunk->next;
//    }

//	return -1;
//}


int get_hash_id_master(uint32_t *hash)
{
    int i;
    //char tmphash[HASHLEN + 1];
    hehas_t *next_chunk = master_chunks.hehas;

    //memcpy(tmphash, hash, HASHLEN);
    //tmphash[HASHLEN] = '\0';

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


int get_hash_id_test(uint32_t *hash)
{
    int i;
    int id;
    char *my_hash;
    //char tmphash[HASHLEN+1];
    char buf[BUFLEN];
    
    FILE *fp = fopen(request_chunks_file, "r");
    
    //memcpy(tmphash, hash, HASHLEN);
    //tmphash[HASHLEN] = '\0';
    
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


void post_send_cleanup(bt_peer_t *peer)
{
    peer->last_ack = 0;
    peer->num_dupacks = 0;
    peer->his_request = NULL;
    me->his_request = NULL;
    peer->send_hash_id = PSUEDO_INF;
    destroy_entries(&send_buffer);
}


void post_receive_cleanup(bt_peer_t *peer)
{
    hehas_t *tmphehas = peer->hehas;
    if (tmphehas != NULL)
    {
        peer->hehas = peer->hehas->next;
        free(tmphehas);
    }
    peer->chunks_fetched++;
    peer->get_hash_id = PSUEDO_INF;
    destroy_entries(&recv_buffer);
}

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
