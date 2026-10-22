import csv
import time
from pathlib import Path

import numpy as np
import openvino as ov

MODEL_PATH = "models/mobilenetv2-12.onnx"
CSV_PATH = Path("../results/gpu_latency.csv")

WARMUP_RUNS = 20
MEASURED_RUNS = 100

print("=== GPU INFERENCE BENCHMARK ===")

core = ov.Core()
model = core.read_model(MODEL_PATH)
compiled = core.compile_model(model, "GPU")

print("Execution device:", compiled.get_property("EXECUTION_DEVICES"))

rng = np.random.default_rng(42)
input_data = rng.random((1, 3, 224, 224), dtype=np.float32)

request = compiled.create_infer_request()
input_port = compiled.input(0)

print("Warm-up runs:", WARMUP_RUNS)

for _ in range(WARMUP_RUNS):
    request.infer({input_port: input_data})

print("Measured runs:", MEASURED_RUNS)

latencies_ms = []

for _ in range(MEASURED_RUNS):
    start = time.perf_counter_ns()

    request.infer({input_port: input_data})

    end = time.perf_counter_ns()

    latencies_ms.append((end - start) / 1_000_000)

times = np.array(latencies_ms)

CSV_PATH.parent.mkdir(parents=True, exist_ok=True)

with CSV_PATH.open("w", newline="") as file:
    writer = csv.writer(file)
    writer.writerow(["run", "latency_ms"])

    for index, latency in enumerate(latencies_ms, start=1):
        writer.writerow([index, f"{latency:.6f}"])

print()
print("=== RESULTS (milliseconds) ===")
print(f"Minimum: {np.min(times):.3f}")
print(f"Mean:    {np.mean(times):.3f}")
print(f"Median:  {np.median(times):.3f}")
print(f"P95:     {np.percentile(times, 95):.3f}")
print(f"P99:     {np.percentile(times, 99):.3f}")
print(f"Maximum: {np.max(times):.3f}")

print()
print("Saved measurements:", CSV_PATH)
print("GPU BENCHMARK: PASS")
