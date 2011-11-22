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
#include "spiffy.h"

typedef struct thread_args
{
	struct sockaddr_in *toaddr;
	get_packet_t *get;
}get_handler_thread_args;

pthread_mutex_t data_ack_lock = PTHREAD_MUTEX_INITIALIZER,get_handler_lock = PTHREAD_MUTEX_INITIALIZER;
int expecting_ack;
int CHUNKS_PER_PACKET = ((BUFLEN - HEADERLEN)/HASHLEN);
char *outfile;
char *has_chunks_file;

void set_files(char *out, char *has)
{
	outfile = out;
	has_chunks_file = has;
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


get_packet_t * create_get(char *hash)
{
	get_packet_t *get = (get_packet_t *)malloc(sizeof(get_packet_t));
	set_header(&(get->header),TYPE_GET,PAD,PAD,sizeof(header_t), sizeof(get_packet_t));
	memcpy(&(get->hash),hash,HASHLEN);
	return get;
}


data_packet_t * create_data(char *data,int seqnum,int size)
{
	data_packet_t *data_p = (data_packet_t *)malloc(sizeof(data_packet_t));
	set_header(&(data_p->header),TYPE_DATA,seqnum,PAD,sizeof(header_t), sizeof(data_packet_t));
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



void whohas_handler(int sock, struct sockaddr_in *toaddr,whohas_packet_t *whohas,chunks_t *local_chunks)
{
	int i,j;
	char hash[HASHLEN],my_hash[HASHLEN];
	ihave_packet_t * ihave;
	chunks_t chunk_hits;

	chunk_hits.num_chunks =0;
	
	for(i=0;i<whohas->chunks.num_chunks;i++)
	{
		memcpy(hash,whohas->chunks.hashes+i*HASHLEN,HASHLEN);
		hash[HASHLEN-1]='\0';
		for(j = 0;j<local_chunks->num_chunks;j++)
		{
			memcpy(my_hash,local_chunks->hashes+j*HASHLEN,HASHLEN);
		    my_hash[HASHLEN-1]='\0';			
			if(strcmp(hash,my_hash) == 0)
			{
				memcpy(chunk_hits.hashes+chunk_hits.num_chunks*HASHLEN,hash,HASHLEN);
				chunk_hits.num_chunks++;
				break;
			}else{
				printf("hash1:%s\nhash2:%s\n",hash,my_hash);
			}
		}
	}			


	if(chunk_hits.num_chunks)
	{
	    ihave=create_ihave(&chunk_hits);	
		spiffy_sendto(sock,(void *)ihave,ihave->header.packet_len,0,(struct sockaddr *)toaddr,sizeof(toaddr));
		free(ihave);
	}	

}


void ihave_handler(int sock, struct sockaddr_in *toaddr,ihave_packet_t *hehas)
{
	
	int i,offset = 0;
	char hash[HASHLEN];
	get_packet_t *get_p;
	printf("node %s:%d has %d chunks:\n",inet_ntoa(toaddr->sin_addr),ntohs(toaddr->sin_port),hehas->chunks.num_chunks);
	for(i=0;i<hehas->chunks.num_chunks;i++)
	{
		memcpy(hash,hehas->chunks.hashes+offset,HASHLEN);
		printf("%s\n",hash);
		offset+=HASHLEN;
		get_p = create_get(hash);
		spiffy_sendto(sock,(void *)get_p,get_p->header.packet_len,0,(struct sockaddr *)toaddr,sizeof(toaddr));
		free(get_p);
	}


}

int get_hash_id(char *hash)
{
	int id;
	char *my_hash;
	char buf[HASHLEN+2];
	FILE *fd = fopen(has_chunks_file,"r");
	while(fgets(buf,HASHLEN+2,fd)!=NULL)
	{
		id = atoi(strtok(buf," "));
		my_hash = strtok(NULL, " ");
		if(strcmp(hash,my_hash) == 0)
		{
			return id;
		}
	}
	return -1;
}

void get_handler(void* args)
{
	get_handler_thread_args *my_args = (get_handler_thread_args *)args;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	int ret = pthread_mutex_trylock(&get_handler_lock);
	
	if(ret != 0)
	{
		denied_packet_t * denied = create_denied();
		spiffy_sendto(sock,(void *)denied,denied->header.packet_len,0,(struct sockaddr *)my_args->toaddr,sizeof(my_args->toaddr));
		free(denied);
		//close(sock);
		return;		
	}

	char data[BUFLEN];
	int left = CHUNKLEN;
	int bytes_read = BUFLEN;
	int hash_id = get_hash_id(my_args->get->hash);
	int seqnum =1;	
	data_packet_t *data_p;
	FILE *master_fd = fopen(MASTER_DATA_FILE,"r");
	fseek(master_fd,hash_id*HASHLEN,SEEK_SET);
	
	while(left)
	{

		pthread_mutex_lock( &data_ack_lock );
		if(left < BUFLEN){ bytes_read = left; }
	    expecting_ack = seqnum;
		fgets(data,bytes_read,master_fd);
		data_p = create_data(data,seqnum++,bytes_read);
 		spiffy_sendto(sock,(void *)data_p,data_p->header.packet_len,0,(struct sockaddr *)my_args->toaddr,sizeof(my_args->toaddr));
		free(data_p);
		left-=bytes_read;	
	}
	
	fclose(master_fd);
	//close(sock);
	pthread_mutex_unlock(&get_handler_lock);

}

void data_handler(int sock, struct sockaddr_in *toaddr,data_packet_t *data_p, char *outfile)
{
	ack_packet_t* ack;
	FILE *out_fd = fopen(outfile,"a");	
	fwrite(data_p->data,1,strlen(data_p->data),out_fd);
	fclose(out_fd);
	ack = create_ack(data_p->header.seq_num);
	spiffy_sendto(sock,(void *)ack,ack->header.packet_len,0,(struct sockaddr *)toaddr,sizeof(toaddr));
	free(ack);

	
}

void ack_handler(pthread_mutex_t *data_ack_lock,ack_packet_t *ack)
{
	if(ack->header.ack_num == expecting_ack)
	{
   		pthread_mutex_unlock( data_ack_lock );

	}
}

void denied_handler(denied_packet_t * denied)
{
	printf("GOT DENIED!!!\n");

}

void packet_handler( struct sockaddr_in *toaddr,void *packet, chunks_t *local_chunks)
{
  	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	header_t header;
	get_header(&header,packet); 
    if (header.packet_type == TYPE_WHOHAS)
    {
		whohas_handler(sock,toaddr,(whohas_packet_t *)packet,local_chunks);
	}else if(header.packet_type == TYPE_IHAVE)
	{
		ihave_handler(sock,toaddr,(ihave_packet_t *)packet);
	}else if(header.packet_type == TYPE_GET)
	{
			pthread_t thread1;
			get_handler_thread_args args;
			args.toaddr = toaddr;
			args.get = (get_packet_t *)packet;
			pthread_create( &thread1, NULL, (void *)&get_handler, (void *)&args);
			//get_handler(sock,toaddr,(get_packet_t *)packet);

	}else if (header.packet_type == TYPE_DATA)
    {
		data_handler(sock,toaddr,(data_packet_t *)packet,outfile);
	}else if (header.packet_type == TYPE_ACK)
	{
		ack_handler(&data_ack_lock,(ack_packet_t *)packet);
	}else 
	{
		denied_handler((denied_packet_t *)packet);
	}

	//close(sock);
	
}



void send_whohas(bt_peer_t *peers, int id, int num_chunks, char *hashes)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(fd != 0);
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
			
			spiffy_sendto(fd,(void *)whohas,whohas->header.packet_len,0,(struct sockaddr *)&(peer->addr),sizeof(peer->addr));
			peer = peer->next;		
		}
		//close(fd);
		free(whohas);	
	}
	
}





