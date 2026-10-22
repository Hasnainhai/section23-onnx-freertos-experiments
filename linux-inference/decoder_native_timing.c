#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "packet_decoder.h"

#define BATCHES 200
#define CALLS_PER_BATCH 2000

_Static_assert(PACKET_SIZE == 12, "Expected 12-byte protocol");

static volatile unsigned long result_sink = 0;

static uint64_t now_ns(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        perror("clock_gettime");
        return 0;
    }

    return (uint64_t)ts.tv_sec * 1000000000ULL +
           (uint64_t)ts.tv_nsec;
}

int main(void)
{
    static const struct
    {
        const char *name;
        uint8_t packet[PACKET_SIZE];
        int expected;
    } cases[] = {
        {
            "valid",
            {0xa5, 0x5a, 0x01, 0x00, 0x7c, 0x03,
             0xf5, 0xd0, 0x4a, 0x01, 0x01, 0xee},
            1
        },
        {
            "bad_checksum",
            {0xa5, 0x5a, 0x02, 0x00, 0x7d, 0x03,
             0x40, 0xe2, 0x01, 0x00, 0x01, 0x20},
            0
        },
        {
            "invalid_class",
            {0xa5, 0x5a, 0x03, 0x00, 0xe8, 0x03,
             0x40, 0xe2, 0x01, 0x00, 0x01, 0xb5},
            0
        }
    };

    FILE *csv = fopen("results/decoder_native_timing.csv", "w");

    if (csv == NULL)
    {
        perror("Opening CSV");
        return 1;
    }

    fprintf(csv, "case,batch,mean_ns_per_call\n");

    printf("=== NATIVE PACKET DECODER TIMING ===\n");
    printf("Clock: CLOCK_MONOTONIC\n");
    printf("Batches per case: %d\n", BATCHES);
    printf("Calls per batch: %d\n", CALLS_PER_BATCH);

    for (unsigned int c = 0;
         c < sizeof(cases) / sizeof(cases[0]); c++)
    {
        PacketResult decoded = {0};

        int actual = packet_decode(cases[c].packet, &decoded);

        if (actual != cases[c].expected)
        {
            fprintf(stderr,
                    "FAIL: %s returned %d, expected %d\n",
                    cases[c].name, actual, cases[c].expected);
            fclose(csv);
            return 1;
        }

        printf("\nCase: %s\n", cases[c].name);
        printf("Correctness check: PASS\n");

        double minimum = 1e30;
        double maximum = 0.0;
        double total = 0.0;

        for (int batch = 0; batch < BATCHES; batch++)
        {
            unsigned long accepted = 0;

            uint64_t start = now_ns();

            for (int i = 0; i < CALLS_PER_BATCH; i++)
            {
                accepted += (unsigned long)
                    packet_decode(cases[c].packet, &decoded);
            }

            uint64_t finish = now_ns();

            result_sink += accepted;

            if (finish < start || start == 0)
            {
                fprintf(stderr, "Timer error\n");
                fclose(csv);
                return 1;
            }

            double ns_per_call =
                (double)(finish - start) / CALLS_PER_BATCH;

            if (ns_per_call < minimum)
                minimum = ns_per_call;

            if (ns_per_call > maximum)
                maximum = ns_per_call;

            total += ns_per_call;

            fprintf(csv, "%s,%d,%.3f\n",
                    cases[c].name, batch + 1, ns_per_call);
        }

        printf("Minimum batch average: %.2f ns/call\n", minimum);
        printf("Mean of batch averages: %.2f ns/call\n",
               total / BATCHES);
        printf("Maximum batch average: %.2f ns/call\n", maximum);
    }

    fclose(csv);

    printf("\nCorrectness checks: ALL PASS\n");
    printf("Timing CSV: results/decoder_native_timing.csv\n");
    printf("Result checksum: %lu\n", result_sink);

    return 0;
}
