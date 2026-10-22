#include <stdint.h>
#include "packet_decoder.h"

int packet_decode(
    const uint8_t packet[PACKET_SIZE],
    PacketResult *result)
{
    uint8_t checksum = 0U;

    if (packet == 0 || result == 0)
    {
        return 0;
    }

    if (packet[0] != PACKET_MAGIC0 ||
        packet[1] != PACKET_MAGIC1)
    {
        return 0;
    }

    for (unsigned int i = 0U; i < PACKET_SIZE; i++)
    {
        checksum ^= packet[i];
    }

    if (checksum != 0U)
    {
        return 0;
    }

    if (packet[PACKET_OFFSET_VALID] > 1U)
    {
        return 0;
    }

    result->sequence =
        (uint16_t)packet[2] |
        ((uint16_t)packet[3] << 8);

    result->class_index =
        (uint16_t)packet[4] |
        ((uint16_t)packet[5] << 8);

    result->timestamp_ms =
        (uint32_t)packet[6] |
        ((uint32_t)packet[7] << 8) |
        ((uint32_t)packet[8] << 16) |
        ((uint32_t)packet[9] << 24);

    result->valid = packet[10];

    if (result->valid == 1U && result->class_index >= 1000U)
    {
        return 0;
    }

    return 1;
}
