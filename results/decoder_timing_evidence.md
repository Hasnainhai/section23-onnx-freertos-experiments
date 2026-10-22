# Packet Decoder: Control-Flow and Dynamic Timing Evidence

## Reference platform
- Host: Native Linux on Intel Core i5-8350U
- RTOS: FreeRTOS ARM Cortex-M3 firmware under QEMU lm3s6965evb
- Function: packet_decode()
- Protocol size: 12 bytes
- Compiled ARM function size: 254 bytes
- ARM instruction set: Thumb-2

## ARM control-flow investigation
QEMU address-filtered execution traces were collected for three
packet types using the optimized FreeRTOS firmware.

| Input case | Decoder result | QEMU block entries | Checksum iterations |
|---|---|---:|---:|
| Valid class 892 | Accepted | 23 | 12 |
| Corrupted checksum | Rejected | 21 | 12 |
| Valid checksum, class 1000 | Rejected | 24 | 12 |

The traces confirm distinct checksum-rejection, class-rejection,
and successful-return paths.

QEMU execution-block entries are not instruction counts,
physical processor cycles, or WCET bounds.

## Native Linux dynamic timing
The unchanged decoder C implementation was compiled for the
native Linux CPU using GCC 13.3.0 with -O0.

Each input case used 200 batches of 2,000 calls.

| Case | Median ns/call | Mean ns/call | P95 | P99 | Observed max |
|---|---:|---:|---:|---:|---:|
| Valid | 60.22 | 70.05 | 117.75 | 148.52 | 175.78 |
| Bad checksum | 34.32 | 35.66 | 47.96 | 56.34 | 59.69 |
| Invalid class | 55.39 | 59.27 | 80.12 | 87.02 | 93.82 |

All three correctness checks passed.

Times represent batch-average nanoseconds per decoder call,
not individually measured call latencies.

## Static WCET status
The checksum loop has a fixed bound of 12 iterations, and the
compiled decoder's control flow was inspected using ARM objdump.

No checked static WCET analyzer was installed. Compatibility
with the ARM Cortex-M3 target and a valid hardware timing model
has not been established.

Consequently, no numerical static Cortex-M3 WCET bound is claimed.
Native Linux timing cannot be substituted for Cortex-M3 WCET.

## Supporting evidence files
- results/decoder_qemu_trace.log
- results/decoder_invalid_trace.log
- results/decoder_invalid_class_trace.log
- results/decoder_native_timing.csv
- results/decoder_native_timing_summary.txt
- freertos-qemu/packet_decoder.c
- freertos-qemu/packet_decoder.h
