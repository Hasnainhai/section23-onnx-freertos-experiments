#ifndef PACKET_DECODER_H
#define PACKET_DECODER_H

#include <stdint.h>
#include "packet_protocol.h"

typedef struct
{
    uint16_t sequence;
    uint16_t class_index;
    uint32_t timestamp_ms;
    uint8_t valid;
} PacketResult;

/* Returns 1 for a valid packet, 0 for an invalid packet. */
int packet_decode(
    const uint8_t packet[PACKET_SIZE],
    PacketResult *result
);

#endif
