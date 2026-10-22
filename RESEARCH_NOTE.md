# Section 2.3: Initial ONNX and FreeRTOS Experiments

## 1. Purpose

Following the suggestion to begin with a small reference platform,
I investigated ONNX inference on a Linux CPU/GPU system and connected
the results to FreeRTOS running on an emulated ARM Cortex-M3.

I also tested one isolated neural-network operation directly under
ARM FreeRTOS, measured static memory reductions, and investigated
execution paths and timing limitations.

These are initial experiments, not a complete deterministic
heterogeneous inference system.

## 2. Experimental platform

- Host: Linux Mint 22.3 on an Intel Core i5-8350U.
- GPU: Intel UHD Graphics 620.
- Model: MobileNetV2 in ONNX format.
- Linux inference runtime: OpenVINO.
- RTOS: FreeRTOS on QEMU's ARM Cortex-M3 lm3s6965evb platform.
- Communication: fixed 12-byte inference-result packets over
  an emulated UART.

The full MobileNetV2 network was executed on Linux, using either
the CPU or GPU. It was not executed on the ARM Cortex-M3.

## 3. CPU and GPU inference

For the synthetic-input benchmark, I used batch size 1,
20 warm-up runs and 100 measured runs per device.
Model compilation was excluded from inference timing.

| Metric | CPU | GPU |
|---|---:|---:|
| Mean latency | 4.443 ms | 9.275 ms |
| Observed maximum | 5.814 ms | 10.767 ms |

On this laptop and workload, CPU inference was faster than GPU
inference. This should not be generalized to other devices or models.

I also ran the model on a real kitten photograph. CPU and GPU
inference completed successfully and agreed on top class index 281
using the tested image preprocessing. This verifies execution and
device agreement, not independently validated classification accuracy.

## 4. Linux to FreeRTOS communication

I transferred fixed 12-byte inference-result packets from Linux
through a QEMU UART to a receiving FreeRTOS task.

Both CPU-generated and GPU-generated packets were accepted
in the completed communication tests.

This demonstrates functional communication between the Linux
inference process and the emulated RTOS component. It does not
establish an end-to-end real-time execution guarantee.

## 5. Periodic inference measurements

I tested 100 periodic inference jobs per device using a
25 ms release period and a 20 ms relative deadline.

| Measurement | CPU | GPU |
|---|---:|---:|
| Mean response time | 15.002 ms | 11.371 ms |
| Maximum observed response | 21.125 ms | 21.243 ms |
| Deadline misses | 4/100 | 1/100 |

These are observed measurements on a best-effort Linux system.
They are not worst-case timing bounds, and they do not include
the complete Linux-to-FreeRTOS communication pipeline.

The different CPU/GPU ordering between this test and the
continuous benchmark illustrates the importance of specifying
the measurement setup.

## 6. ARM FreeRTOS convolution experiment

I extracted one depthwise-convolution channel from the
MobileNetV2 computation and implemented it for the
emulated ARM Cortex-M3 under FreeRTOS.

The experiment used 196 input values, 9 weights and
produced 49 output values.

All 49 ARM results matched the reference outputs.

This is an isolated operator experiment, not deployment
of the full neural network on the microcontroller.

## 7. Static memory experiments

I completed two separate FreeRTOS stack-allocation experiments.

### 7.1 Packet receiver task

Reducing the second receiver task's static stack allocation
from 256 to 192 words reduced firmware static RAM from
4,152 to 3,896 bytes.

Saving: 256 bytes, approximately 6.17%.

### 7.2 Isolated ARM convolution firmware

Reducing the inference task's static stack allocation from
512 to 256 words reduced firmware static RAM from
3,160 to 2,136 bytes.

Saving: 1,024 bytes, approximately 32.4%.

All 49 convolution outputs remained correct after this change.
Observed stack high-water marks indicated remaining headroom,
but do not prove worst-case stack safety.

## 8. Execution-path and timing investigation

I inspected the compiled ARM convolution and supporting
software floating-point routines.

A conditional static path-based analysis produced an estimate
of 174,471 ARM instruction visits. This uses assumptions
about loop bounds in the floating-point helper routines,
including up to 23 normalization-loop iterations.

QEMU instruction traces provided supporting dynamic
execution evidence after interrupt-related repeated entries
were accounted for.

This estimate is **not a validated WCET bound**. It does not
provide a guaranteed number of processor cycles or
microseconds, and it excludes task scheduling and interrupt
interference.

A verified hardware timing model and suitable WCET analysis
toolchain were not available for these experiments.

## 9. Limitations and next steps

- The full ONNX model runs on Linux, not on ARM FreeRTOS.
- The ARM platform is emulated rather than physical hardware.
- CPU and GPU are alternative inference devices in these tests;
  this is not simultaneous workload partitioning.
- Linux latency measurements are observations, not hard
  real-time guarantees.
- Static memory savings are measured firmware allocation
  reductions, not proofs of worst-case stack safety.
- Formal WCET and complete-pipeline deterministic scheduling
  remain future research tasks.

The next stage would be to select a suitable target and timing
model, validate the isolated operator's timing assumptions,
and then consider broader scheduling and partitioning.

## 10. Supporting evidence

The accompanying package will contain selected source files,
Makefiles, original measurement logs, timing CSVs,
the architecture diagram, and four experimental screenshots.

Detailed WCET qualifications are retained in
STATIC_WCET_SCOPE_AND_LIMITATION.md.
