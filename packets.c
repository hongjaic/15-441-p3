#include <assert.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "packets.h"
#include "spiffy.h"

int CHUNKS_PER_PACKET = ((BUFLEN - HEADERLEN)/HASHLEN);

void *bytes_to_packet(char *buf, int size)
{
	void *packet = (void *)malloc(size);
	memcpy(packet,buf,size);	
	return packet;
}

void get_header(header_t *header, void *packet)
{
	int header_len;
	memcpy(header,packet,HEADERLEN);
	header_len = header->header_len;
	if(header_len > HEADERLEN)
	{
		memcpy(header,packet,header_len);
	}
		
}

void set_header(header_t *header,int type, int seq_num, int ack_num, int header_len, int packet_len){

	header->magicnum=MAGIC;
	header->version = VERSION;
	header->packet_type = type;
	header->header_len = header_len;
	header->packet_len = packet_len;
	header->seq_num = seq_num;
	header->ack_num = ack_num;
	
}





ihave_packet_t * create_ihave(chunks_t *chunk_hits)
{

	ihave_packet_t *ihave = (ihave_packet_t *)malloc(sizeof(ihave_packet_t)+chunk_hits->num_chunks*HASHLEN);
	set_header(&(ihave->header),TYPE_IHAVE,PAD,PAD,HEADERLEN, sizeof(ihave_packet_t)+chunk_hits->num_chunks*HASHLEN);
	memcpy(&(ihave->chunks) ,chunk_hits,sizeof(*chunk_hits));

	return ihave;

}


ihave_packet_t * whohas_handler(whohas_packet_t *whohas,chunks_t *local_chunks)
{
	int i,j;
	char hash[HASHLEN],my_hash[HASHLEN];
	ihave_packet_t * ihave;
	chunks_t chunk_hits;

	chunk_hits.num_chunks =0;
	
	for(i=0;i<whohas->chunks.num_chunks;i++)
	{
		memcpy(hash,whohas->chunks.hashes+i*HASHLEN,HASHLEN);
		for(j = 0;j<local_chunks->num_chunks;j++)
		{
			memcpy(my_hash,local_chunks->hashes+j*HASHLEN,HASHLEN);
			if(strcmp(hash,my_hash) == 0)
			{
				memcpy(chunk_hits.hashes+chunk_hits.num_chunks*HASHLEN,hash,HASHLEN);
				chunk_hits.num_chunks++;
			}
		}
	}			

	ihave=create_ihave(&chunk_hits);	
	return ihave;
}


void ihave_handler(struct sockaddr_in *fromaddr,ihave_packet_t *hehas)
{
	int i,offset = 0;
	char hash[HASHLEN];
	printf("node %s has %d chunks:\n",inet_ntoa(fromaddr->sin_addr),hehas->chunks.num_chunks);
	for(i=0;i<hehas->chunks.num_chunks;i++)
	{
		memcpy(hash,hehas->chunks.hashes+offset,HASHLEN);
		printf("%s\n",hash);
		offset+=HASHLEN;
	}


}

void get_handler(get_packet_t *get)
{

}

void data_handler(data_packet_t *data)
{
}

void ack_handler(ack_packet_t *ack)
{
}

void denied_handler(denied_packet_t * denied)
{

}

void packet_handler( struct sockaddr_in *toaddr,void *packet, chunks_t *local_chunks)
{
	header_t header;
	ihave_packet_t * ihave;
  	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	get_header(&header,packet); 
    if (header.packet_type == TYPE_WHOHAS)
    {
		ihave = whohas_handler((whohas_packet_t *)packet,local_chunks);
		if(ihave !=NULL)
		{
			spiffy_sendto(sock,(void *)ihave,ihave->header.packet_len,0,(struct sockaddr *)toaddr,sizeof(toaddr));
		}
	}else if(header.packet_type == TYPE_IHAVE)
	{
		ihave_handler(toaddr,(ihave_packet_t *)packet);
	}else if(header.packet_type == TYPE_GET)
	{
		get_handler((get_packet_t *)packet);
	}else if (header.packet_type == TYPE_DATA)
    {
		data_handler((data_packet_t *)packet);
	}else if (header.packet_type == TYPE_ACK)
	{
		ack_handler((ack_packet_t *)packet);
	}else 
	{
		denied_handler((denied_packet_t *)packet);
	}
    
}





whohas_packet_t * create_whohas(int chunks_num, char *chunk_hashes)
{
 
	whohas_packet_t *whohas = (whohas_packet_t *)malloc(sizeof(whohas_packet_t)+chunks_num*HASHLEN);
	assert(whohas != NULL);

	set_header(&(whohas->header),TYPE_WHOHAS,PAD,PAD,HEADERLEN, sizeof(whohas_packet_t)+chunks_num*HASHLEN);
	whohas->chunks.num_chunks = chunks_num;
	memcpy(whohas->chunks.hashes ,chunk_hashes,HASHLEN*chunks_num);
	return whohas;
}





void send_whohas(bt_peer_t *peers, int id, int num_chunks, char *hashes)
{
	whohas_packet_t *whohas = NULL;
	int iterations = ceil((float)num_chunks/(float)CHUNKS_PER_PACKET);
	int send_count,i;
	bt_peer_t *peer = peers;	
	for(i=0;i<iterations;i++)
	{
		if(iterations == 1)
		{
			send_count = num_chunks;
		}
		else if(iterations != 1 && i ==iterations-1)
		{
			send_count = num_chunks - i*CHUNKS_PER_PACKET;
		}
		else
		{
			send_count = CHUNKS_PER_PACKET;
        }
		whohas=create_whohas(send_count,hashes);

		hashes+=CHUNKS_PER_PACKET;		
	
		while(peer->next!=NULL)
		{
			spiffy_sendto(id,(void *)whohas,whohas->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
			peer = peer->next;		
		}
		free(whohas);	
	}
	
}





