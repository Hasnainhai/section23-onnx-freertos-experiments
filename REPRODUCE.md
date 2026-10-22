# Reproducing the Section 2.3 Initial Experiments

## 1. Overview

This project contains two related but distinct experiments:

1. Full MobileNetV2 ONNX inference on Linux CPU/GPU,
   including communication with an emulated FreeRTOS system.
2. One isolated MobileNetV2 depthwise-convolution channel
   executed on an emulated ARM Cortex-M3 under FreeRTOS.

The complete neural network does not run on the Cortex-M3.

## 2. Original test environment

- Linux Mint 22.3
- Intel Core i5-8350U
- Intel UHD Graphics 620
- Python with OpenVINO 2026.4.1
- MobileNetV2 ONNX model
- GNU Arm Embedded toolchain (arm-none-eabi-gcc)
- QEMU ARM system emulator
- FreeRTOS kernel source

Python dependency versions are recorded in
results/python_requirements.txt.

The GPU experiment requires a compatible GPU and runtime.
Results may differ across hardware and software versions.

## 3. Linux CPU/GPU inference

From the project directory:

    cd linux-inference
    .venv/bin/python cpu_smoke_test.py
    .venv/bin/python gpu_smoke_test.py

These scripts load models/mobilenetv2-12.onnx and run
batch-size-one inference using seeded synthetic input.

The Python environment (.venv) is a local dependency
and is not intended to be included in the final archive.

Original CPU/GPU benchmark results are retained in:

- results/cpu_gpu_comparison.txt
- results/cpu_latency.csv
- results/gpu_latency.csv

Periodic timing measurements are retained separately:

- results/periodic_deadline_validation.txt
- results/periodic_cpu_timing.csv
- results/periodic_gpu_timing.csv

The periodic measurements are observations on Linux,
not guaranteed worst-case execution times.

## 4. ARM FreeRTOS convolution builds

The following directories must be siblings:

    section23-research/
      FreeRTOS-Kernel/
      freertos-qemu/
      linux-inference/

The firmware Makefiles use ../FreeRTOS-Kernel and
../linux-inference as relative source paths.

From the project directory:

    cd freertos-qemu

Build the original isolated convolution firmware:

    make -f Makefile.inference_test

Expected output:

    build_inference_test/inference_test.elf

Build the reduced-stack version:

    make -f Makefile.inference_stackopt

Expected output:

    build_inference_stackopt/inference_stackopt.elf

These commands require arm-none-eabi-gcc, make,
the FreeRTOS kernel sources, startup.s, linker.ld,
and instrumented_config.

The builds use Cortex-M3 Thumb instructions,
software floating point, and -O0.

## 5. ARM results

The original ARM convolution validation is recorded in:

    results/freertos_arm_inference_validation.log

The optimized ARM stack results are recorded in:

    results/arm_inference_stack_optimization_validation.txt

All 49 isolated convolution outputs were verified
against the reference output.

Static RAM decreased from 3,160 to 2,136 bytes
in the separate ARM inference-stack experiment.

The ARM code was evaluated under QEMU, not on
physical Cortex-M3 hardware.

## 6. Linux-to-FreeRTOS communication

Linux CPU/GPU inference results were transmitted
as fixed 12-byte packets through an emulated UART
to a FreeRTOS receiving task.

Selected saved evidence includes:

- results/multipacket_cpu_uart.log
- results/multipacket_gpu_uart.log

The communication experiment and the isolated ARM
convolution experiment are separate workflows.

## 7. WCET and timing limitations

Static execution-path investigation produced a
conditional upper estimate of 174,471 ARM instruction
visits for the isolated convolution and floating-point
helper routines.

This is not a validated WCET in cycles or time.
It depends on stated path assumptions and excludes
interrupt and scheduling interference.

See STATIC_WCET_SCOPE_AND_LIMITATION.md and the
accompanying instruction-analysis evidence.

## 8. Evidence and interpretation

The research note summarizes the experimental findings.
Original logs and CSVs provide the supporting evidence.

The architecture diagram distinguishes full-model Linux
inference from isolated ARM FreeRTOS computation.

A reproduction on different hardware may not yield
identical CPU/GPU latencies or stack observations.

## 9. Setting up Python on another Linux PC

From the extracted project directory:

    cd linux-inference
    python3 -m venv .venv
    .venv/bin/python -m pip install -r ../results/python_requirements.txt

Run the CPU and GPU smoke tests as described in Section 3.
GPU execution requires a supported device and working drivers.
Package installation requires access to the Python dependencies.

The ONNX model is included under linux-inference/models/.
