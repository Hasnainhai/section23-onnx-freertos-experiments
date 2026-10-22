#ifndef PACKET_PROTOCOL_H
#define PACKET_PROTOCOL_H

/* Fixed-size Linux-to-FreeRTOS result packet */
#define PACKET_SIZE 12U

#define PACKET_MAGIC0 0xA5U
#define PACKET_MAGIC1 0x5AU

/* Byte positions, with multibyte fields in little-endian order */
#define PACKET_OFFSET_MAGIC0     0U
#define PACKET_OFFSET_MAGIC1     1U
#define PACKET_OFFSET_SEQUENCE   2U
#define PACKET_OFFSET_CLASS      4U
#define PACKET_OFFSET_TIMESTAMP  6U
#define PACKET_OFFSET_VALID     10U
#define PACKET_OFFSET_CHECKSUM  11U

#endif
