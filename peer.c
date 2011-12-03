/*
 * peer.c
 *
 * Authors:
 *
 *
 */
#include <assert.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "packets.h"
#include "packet_handler.h"
#include "constants.h"
#include "sliding_buffer.h"
#include "congestion_avoidance.h"

#define TO_HEX(line) 0x|line
bt_config_t config;
local_chunks_t local_chunks;
master_chunks_t master_chunks;
char master_data_file[BT_FILENAME_LEN];
char request_chunks_file[BT_FILENAME_LEN];

uint32_t cwnd = 8;
sliding_buffer_t recv_buffer;
sliding_buffer_t send_buffer;
congestion_state_t state;

bt_peer_t *me;	

void peer_run(bt_config_t *config);
void local_chunks_init(char *has_chunk_file);
void master_chunks_init(char *master_chunk_file);


int main(int argc, char **argv) 
{
 	bt_init(&config, argc, argv);	
	bt_parse_command_line(&config);
  	assert(config.has_chunk_file != NULL);
  	bt_dump_config(&config);
  	
	//copy all of my local chunks to memory. 
	local_chunks_init(config.has_chunk_file);
    master_chunks_init(config.chunk_file);

    recv_buffer.num_entry = 0;
    recv_buffer.head = NULL;
    recv_buffer.tail = NULL;

    send_buffer.num_entry = 0;
    send_buffer.head = NULL;
    recv_buffer.tail = NULL;

    init_congestion_state(&state);

  	DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

//#ifdef TESTING
//  config.identity = 1; // your group number here
//  strcpy(config.chunk_file, "chunkfile");
//  strcpy(config.has_chunk_file, "haschunks");
//#endif

  //bt_parse_command_line(&config);

//#ifdef DEBUG
//  if (debug & DEBUG_INIT) {
//    bt_dump_config(&config);
//  }
//#endif
  
  peer_run(&config);
  return 0;
}


/*
opens up the haschunk file for this peer, and copies
all of the local chunk hashes to global variable "local_chunks"
*/
void local_chunks_init(char *has_chunk_file)
{
	int i, j;
    int chunkid;
	char buf[BUFLEN];
	char *hash;
    FILE *fd;
    hehas_t *newchunk;
    hehas_t *append_to;
     
    local_chunks.num_chunks = 0;
    local_chunks.hehas = NULL;

	//set_hasfile(has_chunk_file);
    fd = fopen(has_chunk_file,"r");
	assert(fd != NULL);
    
    while (fgets(buf,BUFLEN,fd) != NULL)
	{
    	local_chunks.num_chunks++;		
	}

	rewind(fd);

    append_to = local_chunks.hehas;

	for(i = 0; i < local_chunks.num_chunks; i++)
	{
		fgets(buf, BUFLEN, fd);
		chunkid = atoi(strtok(buf," "));
		hash = strtok(NULL," ");

        newchunk = (hehas_t *)malloc(sizeof(hehas_t));
        newchunk->chunk_id = chunkid;
        
        for (j = 0; j < 5; j++)
        {
            (newchunk->chunkhash)[j] = hash_to_int(hash, j*8, 8);
        }
        
        newchunk->next = NULL;

        if (append_to == NULL)
        {
            local_chunks.hehas = newchunk;
            append_to = local_chunks.hehas;
        }
        else
        {
            append_to->next = newchunk;
            append_to = newchunk;
        }
	}

	fclose(fd);
}


void master_chunks_init(char *master_chunk_file)
{
	int i, j;
    int chunkid;
	char buf[BUFLEN];
    char *tmp_master_data_file;
	char *hash;
    FILE *fd;
    hehas_t *newchunk;
    hehas_t *append_to;
     
    master_chunks.num_chunks = 0;
    master_chunks.hehas = NULL;

	//set_hasfile(has_chunk_file);
    fd = fopen(master_chunk_file, "r");
	assert(fd != NULL);
    
    while (fgets(buf,BUFLEN,fd) != NULL)
	{
    	master_chunks.num_chunks++;		
	}
    master_chunks.num_chunks -= 2;

	rewind(fd);

    append_to = master_chunks.hehas;

	for(i = 0; i < master_chunks.num_chunks + 2; i++)
	{
        memset(buf, 0, BUFLEN);

        if (i == 0)
        {
            fgets(buf, BUFLEN, fd);
            strtok(buf, " ");
            tmp_master_data_file = strtok(NULL, " ");
            strcpy(master_data_file, tmp_master_data_file);
            master_data_file[strlen(master_data_file)-1] = '\0';
            continue;
        }

        if (i == 1)
        {
            fgets(buf, BUFLEN, fd);
            continue;
        }

		fgets(buf, BUFLEN, fd);
		chunkid = atoi(strtok(buf," "));
		hash = strtok(NULL," ");

        newchunk = (hehas_t *)malloc(sizeof(hehas_t));
        newchunk->chunk_id = chunkid;

        for (j = 0; j < 5; j++)
        {
            (newchunk->chunkhash)[j] = hash_to_int(hash, j*8, 8);
        }

        newchunk->next = NULL;

        if (append_to == NULL)
        {
            master_chunks.hehas = newchunk;
            append_to = master_chunks.hehas;
        }
        else
        {
            append_to->next = newchunk;
            append_to = newchunk;
        }
	}

	fclose(fd);
}


void process_cmd(char *chunkfile, char *outputfile) 
{
    int chunk_num = 0;
	char *hash, *hashes;
	int offset = 0;
	char buf[BUFLEN];
	
    FILE *chunk_fd = fopen(chunkfile, "r");
	assert(chunk_fd!=NULL);	
	
    strcpy(request_chunks_file, chunkfile);
    strcpy(config.output_file, outputfile);

    while(fgets(buf,BUFLEN,chunk_fd) != NULL)
	{
		chunk_num++;
	}
	rewind(chunk_fd);
	
	hashes = (char *)malloc(chunk_num*HASHLEN);
	while(fgets(buf,BUFLEN,chunk_fd) != NULL)
	{
		strtok(buf," ");
		hash = strtok(NULL, " ");
		memcpy(hashes+offset, hash, HASHLEN);
	    offset += HASHLEN;
	}
	fclose(chunk_fd);

	send_whohas((void *)config.peers, config.identity, chunk_num, hashes);
}

void handle_user_input(char *line, void *cbdata) 
{
  char chunkf[128], outf[128];

  bzero(chunkf, sizeof(chunkf));
  bzero(outf, sizeof(outf));

	if (sscanf(line, "GET %120s %120s", chunkf, outf)) 
	{
		if (strlen(outf) > 0) 
		{
	      process_cmd(chunkf, outf);
    	}
  	}
}

void process_inbound_udp(int in_sock,int out_sock) 
{
	struct sockaddr_in from;
	socklen_t fromlen;
	char buf[MAX_PACKET_LEN];
	void *packet;
	int len;
	bt_peer_t *peer;
	
	fromlen = sizeof(from);
	len = spiffy_recvfrom(in_sock, buf, MAX_PACKET_LEN, 0, (struct sockaddr *) &from, &fromlen);	
	packet = bytes_to_packet(buf,len);
	peer = bt_peer_info2(&config, &from);
	packet_handler(out_sock, (void *)peer, packet);
  
}



void peer_run(bt_config_t *config) 
{
    int sock, out_sock; 
	struct sockaddr_in myaddr;
	fd_set readfds;
	struct user_iobuf *userbuf;

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0; 
  
	if ((userbuf = create_userbuf()) == NULL) 
	{
    	perror("peer_run could not allocate userbuf");
    	exit(-1);
  	}
  
  	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1 || (out_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
	{
	    perror("peer_run could not create socket");
	    exit(-1);
	}
  
	bzero(&myaddr, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(config->myport);
    printf("bound to %s port %d\n",inet_ntoa(myaddr.sin_addr),ntohs(myaddr.sin_port));
  	
	if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) 
	{
	    perror("peer_run could not bind socket");
	    exit(-1);
	}
	
  
	spiffy_init(config->identity, (struct sockaddr *)&myaddr, sizeof(myaddr));
  	me = bt_peer_info(config, config->identity);
	me->fetching_or_fetched = NULL;
    me->curr_to = NULL;

    while (1)
	{
    	int nfds;
    	FD_SET(STDIN_FILENO, &readfds);
    	FD_SET(sock, &readfds);

    	nfds = select(sock+1, &readfds, NULL, NULL, &tv);

        if (nfds == 0 && me->curr_to != NULL)
        {  
            printf("timeout occured\n");
            retransmit_data(sock, me->curr_to);
            tv.tv_sec = 3;
            tv.tv_usec = 0;
        }

    	if (nfds > 0) 
		{
            tv.tv_sec = 3;
            tv.tv_usec = 0;
    		
            if (FD_ISSET(sock, &readfds)) 
			{
				process_inbound_udp(sock,out_sock);
                
    		}
    	  
			if (FD_ISSET(STDIN_FILENO, &readfds)) 
			{
				process_user_input(STDIN_FILENO, userbuf, handle_user_input, "Currently unused");
			}
	    }
	}
	//close(sock);
}
	
	
	


