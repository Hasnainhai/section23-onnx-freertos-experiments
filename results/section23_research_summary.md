# Section 2.3 Reference Platform — Experimental Summary

## 1. Objective

Develop and evaluate a small heterogeneous-computing
reference prototype using native Linux CPU/GPU inference
and a FreeRTOS component running on an emulated ARM Cortex-M3.

The purpose is to establish an independently reproducible
experimental foundation before investigating more advanced
partitioning or deterministic scheduling.

## 2. Implemented platform

Host: Linux Mint 22.3, Intel Core i5-8350U,
Intel UHD Graphics 620.

Inference: MobileNetV2 ONNX model, executed using OpenVINO
on either CPU or GPU.

RTOS: ARM Cortex-M3 FreeRTOS kernel under QEMU lm3s6965evb.

Communication: A fixed 12-byte binary inference-result packet
delivered from Linux through QEMU UART to a FreeRTOS task.

Memory allocation: Statically allocated FreeRTOS tasks and stacks.

Both live CPU-to-FreeRTOS and GPU-to-FreeRTOS experiments passed.

## 3. Inference latency

Input: seeded synthetic tensor, batch size 1, 3 x 224 x 224.
Each device: 20 warm-up runs and 100 measured runs.
Model compilation excluded from measured inference latency.

| Metric | CPU | GPU |
|---|---:|---:|
| Mean | 4.443 ms | 9.275 ms |
| Median | 4.363 ms | 9.191 ms |
| P95 | 5.201 ms | 9.702 ms |
| P99 | 5.803 ms | 10.645 ms |
| Observed maximum | 5.814 ms | 10.767 ms |

In this setup, GPU mean latency was 2.09 times CPU mean latency.
This is an observed platform-specific result, not a general
comparison of CPU and GPU performance.

## 4. FreeRTOS static memory optimization

The second FreeRTOS task's static stack allocation was reduced
from 256 words to 192 words.

| Build | .text | .data | .bss |
|---|---:|---:|---:|
| Stack-probe baseline | 16488 B | 4 B | 4148 B |
| Optimized | 16492 B | 4 B | 3892 B |

Static RAM (.data + .bss) decreased from 4152 to 3896 bytes:
256 bytes saved, approximately 6.17%.

Optimized stack high-water marks reported free space of
229 words (Task 1), 167 words (Task 2), and 221 words (UART).

CPU and GPU live-communication tests passed with the
optimized firmware. Measured stack headroom is not a
worst-case stack-usage guarantee.

## 5. Packet decoder and timing investigation

A fixed-size 12-byte ARM packet decoder was inspected.
The Cortex-M3 Thumb-2 binary contains a bounded
12-iteration checksum loop.

QEMU execution traces demonstrated three paths:
- Valid class 892: accepted; 23 execution-block entries.
- Corrupted checksum: rejected; 21 entries.
- Invalid class 1000: rejected; 24 entries.

Each test executed the checksum-loop block 12 times.

Native Linux timing of the same decoder C implementation,
compiled with GCC -O0, used 200 batches of 2000 calls
per input case.

| Case | Mean ns/call | P99 ns/call |
|---|---:|---:|
| Valid | 70.05 | 148.52 |
| Bad checksum | 35.66 | 56.34 |
| Invalid class | 59.27 | 87.02 |

These are native Intel CPU batch-average timings.
QEMU translated-block counts are not hardware cycle counts.

A validated static Cortex-M3 WCET bound has not been
established. Compatible analyzer and hardware timing-model
selection remain future investigation.

## 6. Scope and limitations

This is a small experimental reference platform.
It does not demonstrate physical Cortex-M3 hardware timing,
an NPU, hard hardware isolation, or formal schedulability
guarantees.

CPU and GPU inference were tested as alternative execution
devices, not as simultaneous partitioned workloads.

Inference inputs were synthetic, so these experiments
demonstrate execution and integration rather than
real-image classification accuracy.

## 7. Reproducible evidence

- results/cpu_gpu_comparison.txt
- results/cpu_latency.csv
- results/gpu_latency.csv
- results/stack_probe.log
- results/stack_optimized.log
- results/gpu_stack_optimized.log
- results/decoder_timing_evidence.md
- results/decoder_native_timing.csv
- freertos-qemu/freertos_stack_probe.elf
- freertos-qemu/freertos_stack_optimized.elf

## 8. Next research direction

Evaluate compatible static WCET analysis approaches for
a bounded ARM Cortex-M3 function, with explicitly stated
microarchitectural and memory-timing assumptions.

More advanced static partitioning, hardware deployment,
and deterministic scheduling remain future work.
