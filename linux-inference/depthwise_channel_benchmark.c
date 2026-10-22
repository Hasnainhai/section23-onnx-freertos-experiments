#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "depthwise_channel_fixture.h"

#define SAMPLES 2000
#define WARMUPS 300

extern void depthwise_channel(
    const float input[196],
    const float weights[9],
    float output[49]
);

static double elapsed_us(
    const struct timespec *start,
    const struct timespec *end
) {
    return (double)(end->tv_sec - start->tv_sec)
               * 1000000.0
         + (double)(end->tv_nsec - start->tv_nsec)
               / 1000.0;
}

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a;
    double y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(void)
{
    double samples[SAMPLES];
    float output[49] = {0};
    volatile float sink = 0.0f;
    struct timespec start, end;

    for (int i = 0; i < WARMUPS; ++i) {
        depthwise_channel(test_input, test_weights, output);
    }

    for (int i = 0; i < SAMPLES; ++i) {
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &start) != 0) {
            perror("clock_gettime");
            return 1;
        }

        depthwise_channel(test_input, test_weights, output);

        if (clock_gettime(CLOCK_MONOTONIC_RAW, &end) != 0) {
            perror("clock_gettime");
            return 1;
        }

        samples[i] = elapsed_us(&start, &end);
        sink += output[i % 49];
    }

    double max_error = 0.0;

    for (int i = 0; i < 49; ++i) {
        double error = fabs(
            (double)output[i] - (double)expected_output[i]
        );

        if (!isfinite(output[i]) || error > 1e-5) {
            puts("KERNEL CORRECTNESS: FAIL");
            return 1;
        }

        if (error > max_error) {
            max_error = error;
        }
    }

    qsort(samples, SAMPLES, sizeof(double), compare_double);

    double sum = 0.0;
    for (int i = 0; i < SAMPLES; ++i) {
        sum += samples[i];
    }

    puts("=== REAL MOBILENETV2 INFERENCE KERNEL ===");
    puts("Layer: Conv_75/WithoutBiases, channel 0");
    puts("Architecture: native x86-64");
    puts("Compiler optimization: -O0");
    printf("Warm-up calls: %d\n", WARMUPS);
    printf("Measured calls: %d\n", SAMPLES);
    printf("Maximum output error: %.9g\n", max_error);
    puts("KERNEL CORRECTNESS: PASS");

    puts("\n=== OBSERVED CALL LATENCY ===");
    printf("Minimum (us): %.3f\n", samples[0]);
    printf("Mean (us): %.3f\n", sum / SAMPLES);
    printf("Median (us): %.3f\n", samples[SAMPLES / 2]);
    printf("P95 (us): %.3f\n", samples[1899]);
    printf("P99 (us): %.3f\n", samples[1979]);
    printf("Maximum (us): %.3f\n", samples[SAMPLES - 1]);
    printf("Checksum: %.6f\n", (double)sink);

    puts("\nLIMITATION:");
    puts("Observed execution times include clock-read overhead.");
    puts("These measurements are not guaranteed WCET bounds.");
    puts("They are for the isolated kernel, not full OpenVINO.");

    puts("\nSTEP 155 DYNAMIC KERNEL TIMING: PASS");
    return 0;
}
