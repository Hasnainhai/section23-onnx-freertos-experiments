# Static WCET — Scope and Limitation

Static execution-path analysis was performed for one MobileNetV2
depthwise-convolution channel compiled for ARM Cortex-M3 and executed
under FreeRTOS. All 49 outputs were verified against reference results.

The analysis produced a conditional, path-based upper estimate of
174,471 ARM instruction visits under the stated assumptions. It was
based on modeled control flow and assumed loop limits for the
compiler-provided floating-point helper routines, including a maximum
of 23 normalization-loop iterations. These assumptions have not been
formally verified for all possible inputs. QEMU instruction tracing
supported the convolution instruction accounting after interrupt-related
trace repetitions were reconciled.

This estimate is NOT a validated worst-case execution time (WCET).
It does not provide a bound in processor cycles or microseconds.
It excludes interrupt and scheduling interference, so it represents
a conditional code-path estimate for the analyzed convolution and
its arithmetic helpers, not a task-level or system-level WCET guarantee.

The QEMU environment does not provide a validated cycle-accurate
timing model for the target hardware, and no specialized WCET toolchain
with an appropriate verified Cortex-M3 timing model was available.

Linux CPU/GPU measurements are observed execution behavior, not
guaranteed WCET. In 100 periodic jobs per device, 4 CPU jobs and
1 GPU job missed the 20 ms relative deadline. These measurements
cover Linux inference execution, not end-to-end Linux–FreeRTOS processing.

Validated WCET analysis of the complete inference pipeline remains
future work and would require an appropriate analyzable implementation,
platform timing model, and supporting verification.
