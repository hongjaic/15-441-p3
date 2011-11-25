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
#include "bt_parse.h"
#include "spiffy.h"

FILE *master_fd; 

int CHUNKS_PER_PACKET = ((BUFLEN - HEADERLEN)/HASHLEN);
FILE  *out;
char *has_chunks_file;

void set_hasfile(char *has)
{
	has_chunks_file = has;
}
void set_outfile(FILE *outf)
{
	out= outf;
}

void *bytes_to_packet(char *buf, int size)
{
	void *packet = (void *)malloc(size);
	memcpy(packet,buf,size);	
	return packet;
}


//returns the header of the packet
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


//sets the packet header
void set_header(header_t *header,int type, int seq_num, int ack_num, int header_len, int packet_len){

	header->magicnum=MAGIC;
	header->version = VERSION;
	header->packet_type = type;
	header->header_len = header_len;
	header->packet_len = packet_len;
	header->seq_num = seq_num;
	header->ack_num = ack_num;
	
}



whohas_packet_t * create_whohas(int chunks_num, char *chunk_hashes)
{
 
	whohas_packet_t *whohas = (whohas_packet_t *)malloc(sizeof(whohas_packet_t)+chunks_num*HASHLEN);
	assert(whohas != NULL);

	set_header(&(whohas->header),TYPE_WHOHAS,PAD,PAD,sizeof(header_t), sizeof(whohas_packet_t)+chunks_num*HASHLEN);
	whohas->chunks.num_chunks = chunks_num;
	memcpy(whohas->chunks.hashes ,chunk_hashes,HASHLEN*chunks_num);
	return whohas;
}




ihave_packet_t * create_ihave(chunks_t *chunk_hits)
{

	ihave_packet_t *ihave = (ihave_packet_t *)malloc(sizeof(ihave_packet_t)+chunk_hits->num_chunks*HASHLEN);
	set_header(&(ihave->header),TYPE_IHAVE,PAD,PAD,sizeof(header_t), sizeof(ihave_packet_t)+chunk_hits->num_chunks*HASHLEN);
	memcpy(&(ihave->chunks) ,chunk_hits,sizeof(*chunk_hits));

	return ihave;

}


get_packet_t * create_get(char hash[])
{
	get_packet_t *get = (get_packet_t *)malloc(sizeof(get_packet_t));
	set_header(&(get->header),TYPE_GET,PAD,PAD,sizeof(header_t), sizeof(get_packet_t));
	memcpy(&(get->hash),hash,HASHLEN);
	return get;
}


data_packet_t * create_data(char *data,int seqnum,int size)
{
	data_packet_t *data_p = (data_packet_t *)malloc(sizeof(data_packet_t));
	set_header(&(data_p->header),TYPE_DATA,seqnum,PAD,sizeof(header_t), sizeof(header_t)+size);
	memcpy(&(data_p->data) ,data,size);
	return data_p;
}


ack_packet_t * create_ack(int acknum)
{
	ack_packet_t *ack = (ack_packet_t *)malloc(sizeof(ack_packet_t));
	set_header(&(ack->header),TYPE_ACK,PAD,acknum,sizeof(header_t), sizeof(ack_packet_t));
	return ack;
}

denied_packet_t * create_denied()
{
	denied_packet_t *denied = (denied_packet_t *)malloc(sizeof(denied_packet_t));
	set_header(&(denied->header),TYPE_DENIED,PAD,PAD,sizeof(header_t), sizeof(denied_packet_t));
	return denied;
}



/*
Call whenever this peer receives a whohas packet.
The peer compares all the packets in the whohas payload against
his local chunks and creates a new struct with all the
common chunks. We then create an ihave packet with and return it to the 
source 
*/

void whohas_handler(int sock,bt_peer_t *peer,whohas_packet_t *whohas,chunks_t *local_chunks)
{
	int i,j;
	char hash[HASHLEN+1],my_hash[HASHLEN+1];
	ihave_packet_t * ihave;
	chunks_t chunk_hits;

	chunk_hits.num_chunks =0;
	
	for(i=0;i<whohas->chunks.num_chunks;i++)
	{
		memcpy(hash,whohas->chunks.hashes+i*HASHLEN,HASHLEN);
		hash[HASHLEN]='\0';
		for(j = 0;j<local_chunks->num_chunks;j++)
		{
			memcpy(my_hash,local_chunks->hashes+j*HASHLEN,HASHLEN);
		    my_hash[HASHLEN]='\0';			
			if(strcmp(hash,my_hash) == 0)
			{
				memcpy(chunk_hits.hashes+chunk_hits.num_chunks*HASHLEN,hash,HASHLEN);
				chunk_hits.num_chunks++;
				break;
			}else{
				//printf("hash1:%s\nhash2:%s\n",hash,my_hash);
			}
		}
	}			


	if(chunk_hits.num_chunks)
	{
		
	    ihave=create_ihave(&chunk_hits);	
		//printf("sending %d chunks\n",ihave->chunks.num_chunks);
		spiffy_sendto(sock,(void *)ihave,ihave->header.packet_len,0,(struct sockaddr *)(&peer->addr),sizeof(peer->addr));
		free(ihave);
	}	

}



/*
creates a new "get" packet and asks for the next chunk from peer. 
*/
void get_next_chunk(int sock,bt_peer_t *peer)
{
	char hash[HASHLEN+1];
	get_packet_t *get_p;
	
	memcpy(hash,peer->hehas->chunks.hashes+peer->chunks_fetched*HASHLEN,HASHLEN);
	hash[HASHLEN]='\0';
	printf("Requesting chunk %s\n",hash);
	get_p = create_get(hash);
	spiffy_sendto(sock,(void *)get_p,get_p->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
	free(get_p);
}

/*
Gets called whenever this peer receives an "ihave" packet.
We send a "Get" request for the first chunk in the packet.
This will trigger a chain of events until all the chunks in this ihave packet
have been downloaded.(Currently, we "naively" ask that peer for all of the chunks 
in the "ihave" packet... we want to only ask him for some of them.. to do )

*/

void ihave_handler(int sock,bt_peer_t *peer,ihave_packet_t *hehas)
{
	
	
	printf("node %d has %d chunks:\n",peer->id,hehas->chunks.num_chunks);
	peer->hehas = hehas;
	peer->bytes_received = 0;
	peer->chunks_fetched = 0;


	get_next_chunk(sock,peer);
	

}


/*
given a hash value, this returns the hash ID as it pertains to the master chunks data file
*/
int get_hash_id(char *hash)
{
	int id;
	char *my_hash;
	char buf[BUFLEN];
	FILE *fd = fopen(has_chunks_file,"r");
	assert(fd!=NULL);
	while(fgets(buf,BUFLEN,fd)!=NULL)
	{
		id = atoi(strtok(buf," "));
		my_hash = strtok(NULL, " ");
		my_hash[HASHLEN] = '\0';
		if(strcmp(hash,my_hash) == 0)
		{
			return id;
		}
		buf[0]='\0';
	}
	return -1;
}


/*

This sends the next piece of chunk data from chunk with id = "hash_id" to the peer.
*/
void send_next_data(int sock,bt_peer_t *peer,int hash_id)
{
	int seqnum;
	data_packet_t *data_p;
	char data[BUFLEN];
	int bytes_read = BUFLEN;

	int left = CHUNKLEN - peer->bytes_sent;
	int offset = hash_id*CHUNKLEN + peer->bytes_sent;

	
	printf("chunk: %d - data %d - total %d\n", hash_id*CHUNKLEN,peer->bytes_sent,offset);

	//fseek(master_fd,offset,SEEK_SET);

	if(left < BUFLEN)
	{ 
		bytes_read = left; 
	}

	if(peer->lastack == NULL)
	{
		seqnum=1;	
	}else
	{
		seqnum = peer->lastack->header.ack_num+1;
	}

	fread(data,1,bytes_read,master_fd);
	//fgets(data,bytes_read,master_fd);

	data_p = create_data(data,seqnum,bytes_read);

	
 	spiffy_sendto(sock,(void *)data_p,data_p->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
	peer->bytes_sent+=bytes_read;	
	//printf("sending seq %d,total %d --(%d)\n",seqnum,peer->bytes_sent,bytes_read);
	
	free(data_p);
	
}


/*
Called whenever we recevie a "Get" packet. We check if we are currently 
servicing another get request, if so, send a denial packet.
If we are free, we send the first piece of chunk data with sequence number 1
*/
void get_handler(int sock,bt_peer_t *peer, bt_peer_t *me, get_packet_t *get)
{

	if(me->his_request != NULL)
	{
		printf("DENIALLLL!!!\n");
		denied_packet_t * denied = create_denied();
		spiffy_sendto(sock,(void *)denied,denied->header.packet_len,0,(struct sockaddr *)(&peer->addr),sizeof(peer->addr));
		free(denied);
		return;		
	}
	*(get->hash+HASHLEN)='\0';
	me->his_request = get;
	peer->his_request = get;
	peer->bytes_sent = 0;
	printf("received GET for chunk %s\n",get->hash);
	peer->hash_id = get_hash_id(peer->his_request->hash);

	master_fd = fopen(MASTER_DATA_FILE,"r");
	assert(master_fd != NULL);
	send_next_data(sock,peer,peer->hash_id);


}



/*
Called whenever we receive a "data" packet. we write the data to the output file
we then create and send an ACK. (which will trigger another data packet to be sent.

If we receive the last data packet of a chunk, we create a new "get" request for the next chunk that the peer advertised in his "i have" packet...
*/
void data_handler(int sock,bt_peer_t *peer, data_packet_t *data_p, FILE *out)
{
	int size = data_p->header.packet_len-data_p->header.header_len;
	ack_packet_t* ack;
	
	fwrite(data_p->data,1,size,out);
	peer->bytes_received+=size;
	ack = create_ack(data_p->header.seq_num);
	//if(data_p->header.seq_num % 50 == 0)
	//{
		printf("received seq %d, total %d --(%d)\n",data_p->header.seq_num,peer->bytes_received,size);
	//}    
	spiffy_sendto(sock,(void *)ack,ack->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
		
	if(peer->bytes_received == CHUNKLEN)
	{
		peer->bytes_received=0;
		peer->chunks_fetched++;
		if(peer->chunks_fetched < peer->hehas->chunks.num_chunks)
		{
			get_next_chunk(sock,peer);
		}
		else
		{
			peer->chunks_fetched = 0;
			peer->hehas = NULL;
			fclose(out);
			printf("DONE!\n");

		}

	}else if(peer->bytes_received > CHUNKLEN)
	{
		printf("DAnger will robinson!!!\n");
		exit(1);
	}
	free(ack);

	
}

/*
Called whenever we receive an "ack" packet. we verify that it is the ack we are expecting from that peer. and if so, we send the next piece of data for the chunk requested until we've sent the whole chunk
*/

void ack_handler(int sock,bt_peer_t *peer,bt_peer_t *me, ack_packet_t *ack)
{
	int should_be = 1;
	if(peer->lastack == NULL)
	{
		should_be =1;
	}else
	{
		should_be = peer->lastack->header.ack_num+1;
	}
	if(ack->header.ack_num == should_be)
	{
		//printf("got ack %d\n",ack->header.ack_num);
		if(peer->bytes_sent < CHUNKLEN)
		{
			peer->lastack = ack;
			send_next_data(sock,peer,peer->hash_id);
		}
		else if (peer->bytes_sent == CHUNKLEN)
		{
			
			me->his_request = NULL;
			peer->his_request = NULL;
			peer->lastack = NULL;
			peer->bytes_sent = 0;
			fclose(master_fd);

		}

		else 
		{
			printf("howdyyy hoeee\n");
			exit(1);
		}
	
	}
}


void denied_handler(int sock,bt_peer_t *peer, denied_packet_t * denied)
{
	printf("GOT DENIED!!!\n");

}


/*
This is the genreic handler that gets called whenever we receive a packet.
We do not know the packet type, so here we determine that, and call the appropriate
handler
*/
void packet_handler( int sock, void *peer,void *me,void *packet, chunks_t *local_chunks)
{

	header_t header;
	get_header(&header,packet); 
    if (header.packet_type == TYPE_WHOHAS)
    {
		whohas_handler(sock,(bt_peer_t *)peer,(whohas_packet_t *)packet,local_chunks);
	}else if(header.packet_type == TYPE_IHAVE)
	{
		ihave_handler(sock,(bt_peer_t *)peer,(ihave_packet_t *)packet);
	}else if(header.packet_type == TYPE_GET)
	{		
		get_handler(sock,(bt_peer_t *)peer,(bt_peer_t *)me,(get_packet_t *)packet);
	}else if (header.packet_type == TYPE_DATA)
    {
		data_handler(sock,(bt_peer_t *)peer,(data_packet_t *)packet,out);
	}else if (header.packet_type == TYPE_ACK)
	{
		ack_handler(sock,(bt_peer_t *)peer,(bt_peer_t *)me,(ack_packet_t *)packet);
	}else 
	{
		denied_handler(sock,(bt_peer_t *)peer,(denied_packet_t *)packet);
	}


	
}


/*
Sends a whohas packet, to all the peers. 
*/
void send_whohas(void *peers, int id, int num_chunks, char *hashes)
{
  	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	whohas_packet_t *whohas = NULL;
	int iterations = ceil((float)num_chunks/(float)CHUNKS_PER_PACKET);
	int send_count,i;
	bt_peer_t *peer = (bt_peer_t *)peers;	
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
	
		
		//while(peer->next!=NULL)
		//{
			
		//	spiffy_sendto(sock,(void *)whohas,whohas->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
		//	peer = peer->next;		
		//}
        //
        while(peer != NULL)
        {
            if (peer->id == id)
            {   
                peer = peer->next;
                continue;
            }

			spiffy_sendto(sock,(void *)whohas,whohas->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
            peer = peer->next;
        }
		close(sock);

		free(whohas);	
	}
	
}





