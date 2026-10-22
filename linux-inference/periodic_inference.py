import argparse
import csv
import time
from pathlib import Path

import numpy as np
import openvino as ov

parser = argparse.ArgumentParser()
parser.add_argument("--device", choices=["CPU", "GPU"], required=True)
args = parser.parse_args()

DEVICE = args.device
JOBS = 100
PERIOD_NS = 25_000_000
DEADLINE_NS = 20_000_000

project = Path(__file__).resolve().parent.parent
model_path = project / "linux-inference/models/mobilenetv2-12.onnx"
csv_path = project / f"results/periodic_{DEVICE.lower()}_timing.csv"

print(f"=== PERIODIC {DEVICE} INFERENCE ===", flush=True)

core = ov.Core()
model = core.read_model(str(model_path))
compiled = core.compile_model(model, DEVICE)

rng = np.random.default_rng(42)
input_data = rng.random((1, 3, 224, 224), dtype=np.float32)

request = compiled.create_infer_request()
input_port = compiled.input(0)

for _ in range(20):
    request.infer({input_port: input_data})

print("Execution device:", compiled.get_property("EXECUTION_DEVICES"))
print("Jobs:", JOBS)
print("Period: 25 ms")
print("Relative deadline: 20 ms")

records = []
base_ns = time.perf_counter_ns() + 100_000_000

for job in range(JOBS):
    scheduled_ns = base_ns + job * PERIOD_NS

    remaining_ns = scheduled_ns - time.perf_counter_ns()
    if remaining_ns > 0:
        time.sleep(remaining_ns / 1_000_000_000)

    start_ns = time.perf_counter_ns()
    request.infer({input_port: input_data})
    finish_ns = time.perf_counter_ns()

    execution_ms = (finish_ns - start_ns) / 1_000_000
    start_lateness_ms = (start_ns - scheduled_ns) / 1_000_000
    response_ms = (finish_ns - scheduled_ns) / 1_000_000
    missed = int(finish_ns > scheduled_ns + DEADLINE_NS)

    records.append((
        job + 1,
        execution_ms,
        start_lateness_ms,
        response_ms,
        missed,
    ))

csv_path.parent.mkdir(parents=True, exist_ok=True)

with csv_path.open("w", newline="") as file:
    writer = csv.writer(file)
    writer.writerow([
        "job",
        "inference_ms",
        "start_lateness_ms",
        "response_from_release_ms",
        "deadline_missed",
    ])
    writer.writerows(records)

executions = np.array([r[1] for r in records])
responses = np.array([r[3] for r in records])
misses = sum(r[4] for r in records)

print("\n=== RESULTS ===")
print(f"Inference mean: {np.mean(executions):.3f} ms")
print(f"Inference P99: {np.percentile(executions, 99):.3f} ms")
print(f"Inference max: {np.max(executions):.3f} ms")
print(f"Response P99: {np.percentile(responses, 99):.3f} ms")
print(f"Response max: {np.max(responses):.3f} ms")
print(f"Deadline misses: {misses} / {JOBS}")
print("Timing data saved:", csv_path)
print("PERIODIC INFERENCE EXPERIMENT: COMPLETED")
