/**
 * CS 15-441 Computer networks
 *
 * Collection of all constants.
 *
 * @file    constants.h
 * @author  Hong Jai Cho <hongjaic>, Raul Gonzalez <rggonzal>
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H


#define HEADERLEN   		16
#define HASHLEN			    40
#define BUFLEN              1500-HEADERLEN   

#define CHUNKLEN			524288
#define PAD					0
#define MAGIC				15441
#define VERSION				1


#define TYPE_WHOHAS         'a'
#define TYPE_IHAVE          'b'
#define TYPE_GET            'c'
#define TYPE_DATA           'd'
#define TYPE_ACK            'e'
#define TYPE_DENIED         'f'

#define MASTER_DATA_FILE    "tmp/wow"



#endif

