#include <assert.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "packets.h"
#include "bt_parse.h"
#include "spiffy.h"



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



void whohas_handler(bt_peer_t *peer, int sock,whohas_packet_t *whohas,chunks_t *local_chunks)
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
		printf("sending %d chunks\n",ihave->chunks.num_chunks);
		spiffy_sendto(sock,(void *)ihave,ihave->header.packet_len,0,(struct sockaddr *)(&peer->addr),sizeof(peer->addr));
		free(ihave);
	}	

}

void get_next_chunk(bt_peer_t *peer,int sock)
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


void ihave_handler(bt_peer_t *peer,int sock,ihave_packet_t *hehas)
{
	
	
	printf("node %d has %d chunks:\n",peer->id,hehas->chunks.num_chunks);
	peer->hehas = hehas;
	peer->bytes_received = 0;
	peer->chunks_fetched = 0;


	get_next_chunk(peer,sock);
	

}

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

void send_next_data(bt_peer_t *peer,int sock,int hash_id)
{
	int seqnum;
	data_packet_t *data_p;
	char data[BUFLEN];
	int bytes_read = BUFLEN;

	int left = CHUNKLEN - peer->bytes_sent;
	int offset = hash_id*HASHLEN + peer->bytes_sent;
	printf("offset: %d\n",offset);
	FILE *master_fd = fopen(MASTER_DATA_FILE,"r");
	assert(master_fd!=NULL);
	fseek(master_fd,offset,SEEK_SET);


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


	fgets(data,bytes_read,master_fd);
	fclose(master_fd);
	data_p = create_data(data,seqnum,bytes_read);

 	spiffy_sendto(sock,(void *)data_p,data_p->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
	free(data_p);
	peer->bytes_sent+=bytes_read;	
	printf("sending seq %d,total %d --(%d)\n",seqnum,peer->bytes_sent,bytes_read);

}

void get_handler(bt_peer_t *peer, bt_peer_t *me, get_packet_t *get)
{

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(me->his_request != NULL)
	{
		denied_packet_t * denied = create_denied();
		spiffy_sendto(sock,(void *)denied,denied->header.packet_len,0,(struct sockaddr *)(&peer->addr),sizeof(peer->addr));
		free(denied);
		
		//close(sock);
		return;		
	}
	*(get->hash+HASHLEN)='\0';
	me->his_request = get;
	peer->his_request = get;
	peer->bytes_sent = 0;
	printf("received GET for chunk %s\n",get->hash);
	peer->hash_id = get_hash_id(peer->his_request->hash);

	
	send_next_data(peer,sock,peer->hash_id);
	//close(sock);

}

void data_handler(bt_peer_t *peer,int sock, data_packet_t *data_p, FILE *out)
{
	int size = data_p->header.packet_len-data_p->header.header_len;
	ack_packet_t* ack;
	
	fwrite(data_p->data,1,size,out);
	peer->bytes_received+=size;
	ack = create_ack(data_p->header.seq_num);
	spiffy_sendto(sock,(void *)ack,ack->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
	printf("received seq %d, total %d --(%d)\n",data_p->header.seq_num,peer->bytes_received,size);	
	if(peer->bytes_received == CHUNKLEN)
	{
		peer->bytes_received=0;
		peer->chunks_fetched++;
		if(peer->chunks_fetched < peer->hehas->chunks.num_chunks)
		{

			get_next_chunk(peer,sock);
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
		//exit(1);
	}
	free(ack);

	
}


void ack_handler(bt_peer_t *peer,bt_peer_t *me, ack_packet_t *ack)
{
	
  	int sock = socket(AF_INET, SOCK_DGRAM, 0);
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
		if(peer->bytes_sent < CHUNKLEN)
		{
			printf("got ack %d\n",ack->header.ack_num);
			peer->lastack = ack;
			send_next_data(peer,sock,peer->hash_id);
		}
		else if (peer->bytes_sent == CHUNKLEN)
		{
			me->his_request = NULL;
			peer->his_request = NULL;
			peer->lastack = NULL;
			peer->bytes_sent = 0;

		}

		else 
		{
			printf("howdyyy hoeee\n");
			exit(1);
		}
	
	}
	//close(sock);
}


void denied_handler(bt_peer_t *peer, denied_packet_t * denied)
{
	printf("GOT DENIED!!!\n");

}

void packet_handler( void *peer,void *me,void *packet, chunks_t *local_chunks)
{
  	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	header_t header;
	get_header(&header,packet); 
    if (header.packet_type == TYPE_WHOHAS)
    {
		whohas_handler((bt_peer_t *)peer,sock,(whohas_packet_t *)packet,local_chunks);
	}else if(header.packet_type == TYPE_IHAVE)
	{
		ihave_handler((bt_peer_t *)peer,sock,(ihave_packet_t *)packet);
	}else if(header.packet_type == TYPE_GET)
	{		
		get_handler((bt_peer_t *)peer,(bt_peer_t *)me,(get_packet_t *)packet);
	}else if (header.packet_type == TYPE_DATA)
    {
		data_handler((bt_peer_t *)peer,sock,(data_packet_t *)packet,out);
	}else if (header.packet_type == TYPE_ACK)
	{
		ack_handler((bt_peer_t *)peer,(bt_peer_t *)me,(ack_packet_t *)packet);
	}else 
	{
		denied_handler((bt_peer_t *)peer,(denied_packet_t *)packet);
	}

	//close(sock);
	
}



void send_whohas(void *peers, int id, int num_chunks, char *hashes)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(fd != 0);
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
	
		
		while(peer->next!=NULL)
		{
			
			spiffy_sendto(fd,(void *)whohas,whohas->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
			peer = peer->next;		
		}
		//close(fd);
		free(whohas);	
	}
	
}





