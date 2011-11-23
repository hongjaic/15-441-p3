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

#define TO_HEX(line) 0x|line
bt_config_t config;
chunks_t local_chunks;

void peer_run(bt_config_t *config);
void local_chunks_init(char *has_chunk_file);
int main(int argc, char **argv) {



 	bt_init(&config, argc, argv);	
	bt_parse_command_line(&config);
  	assert(config.has_chunk_file != NULL);
  	bt_dump_config(&config);
  	local_chunks_init(config.has_chunk_file);
  	DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

#ifdef TESTING
  config.identity = 1; // your group number here
  strcpy(config.chunk_file, "chunkfile");
  strcpy(config.has_chunk_file, "haschunks");
#endif

  bt_parse_command_line(&config);

#ifdef DEBUG
  if (debug & DEBUG_INIT) {
    bt_dump_config(&config);
  }
#endif
  
  peer_run(&config);
  return 0;
}




void local_chunks_init(char *has_chunk_file)
{
	int i;
	char buf[BUFLEN];
	FILE *fd = fopen(has_chunk_file,"r");
	assert(fd != NULL);
	char *hash;
    local_chunks.num_chunks = 0;
	set_hasfile(has_chunk_file);
	while(fgets(buf,BUFLEN,fd) != NULL)
	{
    	local_chunks.num_chunks++;		
	}
	rewind(fd);

	for(i=0;i<local_chunks.num_chunks;i++)
	{
		fgets(buf,BUFLEN,fd);
		strtok(buf," ");
		hash = strtok(NULL," ");
		memcpy(local_chunks.hashes+i*HASHLEN,hash,HASHLEN);
	}
	fclose(fd);
	
}





void process_cmd(char *chunkfile, char *outputfile) {

	FILE *chunk_fd = fopen(chunkfile,"r");
	assert(chunk_fd!=NULL);	
	FILE *out_fd = fopen(outputfile,"a");	
	assert(out_fd!=NULL);	
	int chunk_num = 0;
	char *hash, *hashes;
	int offset = 0;
	char buf[BUFLEN];
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
		memcpy(hashes+offset,hash,HASHLEN);
	    offset+=HASHLEN;
	}
	fclose(chunk_fd);


	set_outfile(out_fd);	
	send_whohas((void *)config.peers,config.identity,chunk_num,hashes);


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

void process_inbound_udp(bt_peer_t *me,int sock) 
{
	struct sockaddr_in from;
	socklen_t fromlen;
	char buf[BUFLEN];
	void *packet;
	int len;
	bt_peer_t *peer;
	
	fromlen = sizeof(from);
	len = spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);	
	packet = bytes_to_packet(buf,len);
	peer = bt_peer_info2(&config, &from);
	packet_handler((void *)peer,(void *)me,packet,&local_chunks);
  
}



void peer_run(bt_config_t *config) 
{
  int sock;
	struct sockaddr_in myaddr;
	fd_set readfds;
	struct user_iobuf *userbuf;
	bt_peer_t *me;	
  
	if ((userbuf = create_userbuf()) == NULL) 
	{
    	perror("peer_run could not allocate userbuf");
    	exit(-1);
  	}
  
  	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) 
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
	while (1)
	{
    	int nfds;
    	FD_SET(STDIN_FILENO, &readfds);
    	FD_SET(sock, &readfds);

    	nfds = select(sock+1, &readfds, NULL, NULL, NULL);

    	if (nfds > 0) 
		{

    		if (FD_ISSET(sock, &readfds)) 
			{
				process_inbound_udp(me, sock);
    		}
    	  
			if (FD_ISSET(STDIN_FILENO, &readfds)) 
			{
				process_user_input(STDIN_FILENO, userbuf, handle_user_input, "Currently unused");
			}
	    }
	}
}
	
	
	


