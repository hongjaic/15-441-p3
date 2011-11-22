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
#define BUFLEN              1500   
#define HASHLEN			    40
#define PAD					0
#define MAGIC				15441
#define VERSION				1


#define TYPE_WHOHAS         0x00000000
#define TYPE_IHAVE          0x00000001
#define TYPE_GET            0x00000010
#define TYPE_DATA           0x00000011
#define TYPE_ACK            0x00000100
#define TYPE_DENIED         0x00000101



#endif

