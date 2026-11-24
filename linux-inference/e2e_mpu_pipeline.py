import csv
import socket
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
import openvino as ov
from packet_codec import encode_packet

project = Path(__file__).resolve().parent.parent
firmware = project / "freertos-qemu/build_e2e_mpu/e2e_mpu.elf"
model_path = project / "linux-inference/models/mobilenetv2-12.onnx"
csv_out = project / "results/e2e_mpu_pipeline_latency.csv"

NUM_JOBS = 20
PERIOD_MS = 25.0
DEADLINE_MS = 20.0

core = ov.Core()
model = core.read_model(str(model_path))
compiled = core.compile_model(model, "CPU")
request = compiled.create_infer_request()

rng = np.random.default_rng(123)
input_data = rng.random((1, 3, 224, 224), dtype=np.float32)

# Warm-up OpenVINO
for _ in range(5):
    request.infer({compiled.input(0): input_data})

with tempfile.TemporaryDirectory(prefix="e2e-mpu-") as folder:
    socket_path = str(Path(folder) / "uart.sock")
    cmd = [
        "qemu-system-arm",
        "-M", "lm3s6965evb",
        "-display", "none",
        "-monitor", "none",
        "-kernel", str(firmware),
        "-semihosting-config", "enable=on,target=native",
        "-chardev", f"socket,id=uart0,path={socket_path},server=on,wait=off",
        "-serial", "chardev:uart0",
    ]

    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1
    )

    try:
        for _ in range(50):
            if Path(socket_path).exists():
                break
            time.sleep(0.05)

        # Wait for E2E_MPU_RTOS_READY
        while True:
            line = proc.stdout.readline()
            if "E2E_MPU_RTOS_READY" in line:
                break

        rows = []
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
            client.settimeout(5.0)
            client.connect(socket_path)
            time.sleep(0.05)

            for seq in range(1, NUM_JOBS + 1):
                t_release = time.perf_counter()

                # Stage 1: Linux OpenVINO MobileNetV2 ONNX Inference
                t_inf0 = time.perf_counter()
                outputs = request.infer({compiled.input(0): input_data})
                out_arr = np.asarray(outputs[compiled.output(0)])
                class_idx = int(np.argmax(out_arr[0]))
                t_inf1 = time.perf_counter()

                # Stage 2: 12-byte UART Packet Transmission to FreeRTOS
                ts_ms = (time.monotonic_ns() // 1_000_000) & 0xFFFFFFFF
                pkt = encode_packet(
                    sequence=seq, class_index=class_idx, timestamp_ms=ts_ms, valid=1
                )
                client.sendall(pkt)

                # Stage 3: Wait for MPU-Isolated FreeRTOS INT8 Kernel ACK
                ack_line = ""
                while True:
                    line = proc.stdout.readline()
                    if not line:
                        break
                    if f"E2E_ACK seq={seq} " in line:
                        ack_line = line.strip()
                        break

                t_done = time.perf_counter()

                linux_ms = (t_inf1 - t_inf0) * 1000.0
                rtos_uart_ms = (t_done - t_inf1) * 1000.0
                e2e_ms = (t_done - t_release) * 1000.0
                missed = int(e2e_ms > DEADLINE_MS)

                rows.append((seq, class_idx, round(linux_ms, 3), round(rtos_uart_ms, 3), round(e2e_ms, 3), missed))
                print(f"Job {seq:02d}: class={class_idx} linux={linux_ms:.3f}ms uart+rtos={rtos_uart_ms:.3f}ms e2e={e2e_ms:.3f}ms | {ack_line}")

                # Enforce 25 ms periodic release
                elapsed = (time.perf_counter() - t_release) * 1000.0
                if elapsed < PERIOD_MS:
                    time.sleep((PERIOD_MS - elapsed) / 1000.0)

        with open(csv_out, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["seq", "class_index", "linux_openvino_ms", "uart_plus_rtos_mpu_ms", "e2e_total_ms", "deadline_miss_20ms"])
            w.writerows(rows)

        e2e_vals = [r[4] for r in rows]
        misses = sum(r[5] for r in rows)
        print("\n=== CONCRETE END-TO-END PIPELINE SUMMARY (20 Periodic Jobs @ 25ms) ===")
        print(f"Mean E2E Latency : {np.mean(e2e_vals):.3f} ms")
        print(f"Max E2E Latency  : {np.max(e2e_vals):.3f} ms")
        print(f"Deadline Misses  : {misses}/{NUM_JOBS} (Deadline = {DEADLINE_MS} ms)")
        print(f"Saved CSV        : {csv_out}")
    finally:
        proc.terminate()
        proc.communicate(timeout=5)
