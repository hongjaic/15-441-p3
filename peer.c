/**
 * CS 15-441 Computer Networks
 * Project: Congestion Control with BitTorrent
 * 
 * @file    peer.c
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
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "packets.h"
#include "packet_handler.h"
#include "constants.h"
#include "sliding_buffer.h"
#include "congestion_avoidance.h"
#include "bt_structs.h"

/* Configuration data structures */
bt_config_t config;
local_chunks_t local_chunks;
master_chunks_t master_chunks;
char master_data_file[BT_FILENAME_LEN];
char request_chunks_file[BT_FILENAME_LEN];

/* Congestion controllers */
sliding_buffer_t send_buffer;
congestion_state_t state;

/* This peer */
bt_peer_t *me;	

void peer_run(bt_config_t *config);
void local_chunks_init(char *has_chunk_file);
void master_chunks_init(char *master_chunk_file);
void peer_total_cleanup(bt_peer_t *peer);

int main(int argc, char **argv) 
{
    bt_init(&config, argc, argv);	
    bt_parse_command_line(&config);
    assert(config.has_chunk_file != NULL);
    bt_dump_config(&config);

    // Copy local chunks to memory.
    local_chunks_init(config.has_chunk_file);
    // Copy master chunks to memoery.
    master_chunks_init(config.chunk_file);

    // Initialize the windows
    send_buffer.num_entry = 0;
    send_buffer.head = NULL;
    send_buffer.tail = NULL;

    init_congestion_state(&state);

    DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

    peer_run(&config);

    return 0;
}


/**
 * local_chunks_init -
 * Copies the local chunks into memory.
 *
 * @param has_chunk_file    the file of local chunks
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


/**
 * master_chunks_init -
 * Copies the master chunks into memory.
 *
 * @param master_chunk_file the mast chunks file
 */
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


/**
 * process_cmd -
 * Inpects the command liens input arguments and send whohas packets to peers if
 * the file is in the system.
 *
 * @param chunkfile     the requested chunks
 * @param outputfile    the output file to download into
 */
void process_cmd(char *chunkfile, char *outputfile) 
{
    int chunk_num = 0;
    char *hash, *hashes;
    int offset = 0;
    char buf[BUFLEN];
    char *ext;
    char temp_chunkfile[BUFLEN];

    strcpy(temp_chunkfile, chunkfile);
    strtok(temp_chunkfile, ".");
    ext = strtok(NULL, ".");

    if (strcmp(ext, "chunks") != 0)
    {
        fprintf(stderr, "File name must have extension .chunks\n");
        return;
    }

    FILE *chunk_fd = fopen(chunkfile, "r");

    if (chunk_fd == NULL)
    {
        fprintf(stderr, "The file you're looking for does not exist.\n");
        return;
    }

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

    init_whohas(config.peers, chunk_num, hashes, config.identity);
}


/**
 * handle_user_input -
 * Parses the uer inputs into file names.
 *
 * @param line      user command input
 * @param cbdata    cbdata
 */
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


/**
 * process_inboud_udp -
 * Redirects received packet to packet handler.
 *
 * @param in_sock   read socekt
 * @param out_sock  write socket
 */
void process_inbound_udp(int in_sock, int out_sock) 
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


/**
 * peer_run -
 * The main runnng loop that handles all incoming request and user inputs.
 *
 * @param config    configuration of this peer
 */
void peer_run(bt_config_t *config) 
{
    int sock, out_sock; 
    struct sockaddr_in myaddr;
    fd_set readfds;
    struct user_iobuf *userbuf;
    bt_peer_t *curr_peer;

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

        // Timeout
        if (nfds == 0)
        {
            // If timeout occurred durring transmission of data packets,
            // retransmit.
            if (me->curr_to != NULL)
            {
                if (me->curr_to->num_data_retransmits == MAX_DATA_RETRANSMIT)
                {
                    fprintf(stderr, "Attempted 10 data retransmits, target node: %d is probably down.\n", me->curr_to->id);
                    peer_total_cleanup(me->curr_to);
                    destroy_entries(&send_buffer);
                    me->curr_to = NULL;
                    me->his_request = NULL;
                }
                else
                {
                    retransmit_data(sock, me->curr_to);
                    me->curr_to->num_data_retransmits++;
                }
            }

            // If timeout occurrced for none-data packets, retransmit the
            // corresponding packet.
            curr_peer = config->peers;
            while (curr_peer != NULL)
            {
                if (curr_peer->id == config->identity)
                {
                    curr_peer = curr_peer->next;
                    continue;
                }

                if (curr_peer->num_retransmits == MAX_RETRANSMITS)
                {
                    fprintf(stderr, "Attempted 5 none-data packet retransmits, target node: %d is probably down.\n", curr_peer->id);
                    peer_total_cleanup(curr_peer);
                    me->curr_to = NULL;
                    me->his_request = NULL;
                }

                if (curr_peer->pending_whohas != NULL)
                {
                    retransmit_whohas(sock, curr_peer);
                    curr_peer->num_retransmits++;
                }
                else if (curr_peer->pending_ihave != NULL)
                {
                    retransmit_ihave(sock, curr_peer);
                    curr_peer->num_retransmits++;
                }
                else if (curr_peer->pending_get != NULL)
                {
                    retransmit_get(sock, curr_peer);
                    curr_peer->num_retransmits++;
                }

                if (curr_peer->get_hash_id != PSUEDO_INF)
                {
                    if (curr_peer->ack_timeout_counter == MAX_DATA_RETRANSMIT)
                    {
                        fprintf(stderr, "Sending  node: %d is probably down, no data packet for a while.\n", curr_peer->id);
                        me->chunks_fetched += curr_peer->num_chunks - curr_peer->chunks_fetched;

                        if (me->chunks_fetched == me->num_chunks)
                        {
                            me->chunks_fetched = 0;
                            me->num_chunks = 0;
                        }
                        peer_total_cleanup(curr_peer);
                        destroy_entries(&(curr_peer->recv_buffer));
                        destroy_fof(me);
                    }
                    else
                    {
                        curr_peer->ack_timeout_counter++;
                    }
                }

                curr_peer = curr_peer->next;
            }

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


/**
 * peer_total_cleanup -
 * This function initializes state if the peer that the requests are sent to
 * seems to be dead.
 *
 * @param peer  peer that the request was made to
 */
void peer_total_cleanup(bt_peer_t *peer)
{
    peer->num_chunks = 0;
    peer->chunks_fetched = 0;
    peer->last_ack = 0;
    peer->num_dupacks = 0;

    if (peer->his_request != NULL)
    {
        free(peer->his_request);
        peer->his_request = NULL;
    }

    peer->send_hash_id = PSUEDO_INF;
    peer->get_hash_id = PSUEDO_INF;

    destroy_hehas(peer);
    destroy_whohas_list(peer);

    peer->pending_whohas = NULL;

    if (peer->pending_ihave != NULL)
    {
        free(peer->pending_ihave);
        peer->pending_ihave = NULL;
    }

    if (peer->pending_get != NULL)
    {
        free(peer->pending_get);
        peer->pending_get = NULL;
    }

    peer->num_retransmits = 0;
    peer->num_data_retransmits = 0;
    peer->ack_timeout_counter = 0;
}

