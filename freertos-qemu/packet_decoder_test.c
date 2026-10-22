#include <stdint.h>
#include <stdio.h>
#include "packet_decoder.h"

int main(void)
{
    uint8_t packet[PACKET_SIZE] = {
        0xA5, 0x5A, 0x01, 0x00,
        0x7C, 0x03, 0x40, 0xE2,
        0x01, 0x00, 0x01, 0x23
    };

    PacketResult result = {0};

    printf("=== VALID PACKET TEST ===\n");

    if (packet_decode(packet, &result) != 1)
    {
        printf("VALID PACKET TEST: FAIL\n");
        return 1;
    }

    printf("Sequence: %u\n", (unsigned)result.sequence);
    printf("Class index: %u\n", (unsigned)result.class_index);
    printf("Timestamp: %lu\n", (unsigned long)result.timestamp_ms);
    printf("Valid: %u\n", (unsigned)result.valid);

    if (result.sequence != 1 ||
        result.class_index != 892 ||
        result.timestamp_ms != 123456 ||
        result.valid != 1)
    {
        printf("DECODED VALUES: FAIL\n");
        return 1;
    }

    printf("VALID PACKET TEST: PASS\n");

    printf("\n=== CORRUPTED CHECKSUM TEST ===\n");

    packet[4] ^= 0x01;

    if (packet_decode(packet, &result) != 0)
    {
        printf("CORRUPTED CHECKSUM TEST: FAIL\n");
        return 1;
    }

    printf("CORRUPTED CHECKSUM TEST: PASS\n");

    printf("\n=== INVALID START MARKER TEST ===\n");

    packet[4] ^= 0x01;
    packet[0] = 0x00;

    if (packet_decode(packet, &result) != 0)
    {
        printf("INVALID START MARKER TEST: FAIL\n");
        return 1;
    }

    printf("INVALID START MARKER TEST: PASS\n");
    printf("\nALL PACKET DECODER TESTS: PASS\n");

    return 0;
}
