/**
 * CS 15-441 Computer networks
 * Project: Congestion Control with BitTorrent
 *
 * Collection of all constants.
 *
 * @file    constants.h
 * @author  Hong Jai Cho <hongjaic>, Raul Gonzalez <rggonzal>
 */

#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_


#define MAX_RETRANSMITS     3

#define PSUEDO_INF          (1<<30)

#define HEADERLEN   		16
#define HASHLEN			    40
#define HASHLEN_PACKET      20
#define BUFLEN              1484
#define MAX_PACKET_LEN      1500
#define FINALACK            354

#define CHUNKLEN			524288
#define PAD					0
#define MAGIC				15441
#define VERSION				1


#define TYPE_WHOHAS         0
#define TYPE_IHAVE          1
#define TYPE_GET            2
#define TYPE_DATA           3
#define TYPE_ACK            4
#define TYPE_DENIED         5

#define MAXWIN              64
#define IN_SS               1
#define IN_CA               0
#define TIMEOUT             3

#define MAX_CHUNKS_PER_PACK 74

#define BT_FILENAME_LEN     255
#define BT_MAX_PEERS        1024


#endif /* _CONSTANTS_H_ */

