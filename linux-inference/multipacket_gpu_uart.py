import re
import socket
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
import openvino as ov

from packet_codec import encode_packet

project = Path(__file__).resolve().parent.parent

firmware = (
    project
    / "freertos-qemu/build_periodic/freertos_periodic.elf"
)

model_path = (
    project
    / "linux-inference/models/mobilenetv2-12.onnx"
)

PACKET_COUNT = 5

if not firmware.is_file():
    raise SystemExit("STOP: Periodic FreeRTOS firmware missing.")

if not model_path.is_file():
    raise SystemExit("STOP: MobileNetV2 model missing.")

print("=== MULTI-PACKET GPU INFERENCE PIPELINE ===", flush=True)

core = ov.Core()
model = core.read_model(str(model_path))
compiled = core.compile_model(model, "GPU")
request = compiled.create_infer_request()

print(
    "Execution device:",
    compiled.get_property("EXECUTION_DEVICES"),
    flush=True
)

rng = np.random.default_rng(42)

expected = []

with tempfile.TemporaryDirectory(
    prefix="section23-multipacket-"
) as folder:

    socket_path = str(Path(folder) / "uart.sock")

    command = [
        "qemu-system-arm",
        "-M", "lm3s6965evb",
        "-display", "none",
        "-monitor", "none",
        "-kernel", str(firmware),
        "-semihosting-config", "enable=on,target=native",
        "-chardev",
        f"socket,id=uart0,path={socket_path},server=on,wait=off",
        "-serial", "chardev:uart0",
    ]

    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    sent = 0
    test_error = None

    try:
        for _ in range(50):
            if Path(socket_path).exists():
                break

            if process.poll() is not None:
                raise RuntimeError("QEMU exited unexpectedly.")

            time.sleep(0.1)
        else:
            raise RuntimeError("UART socket was not created.")

        time.sleep(1.0)

        with socket.socket(
            socket.AF_UNIX,
            socket.SOCK_STREAM
        ) as client:

            client.settimeout(5.0)
            client.connect(socket_path)

            for sequence in range(1, PACKET_COUNT + 1):
                input_data = rng.random(
                    (1, 3, 224, 224),
                    dtype=np.float32
                )

                outputs = request.infer({
                    compiled.input(0): input_data
                })

                output = np.asarray(
                    outputs[compiled.output(0)]
                )

                if output.shape != (1, 1000):
                    raise RuntimeError(
                        f"Unexpected output shape: {output.shape}"
                    )

                if not np.isfinite(output).all():
                    raise RuntimeError(
                        "Inference produced nonfinite values."
                    )

                class_index = int(np.argmax(output[0]))

                timestamp_ms = (
                    time.monotonic_ns() // 1_000_000
                ) & 0xFFFFFFFF

                packet = encode_packet(
                    sequence=sequence,
                    class_index=class_index,
                    timestamp_ms=timestamp_ms,
                    valid=1,
                )

                if len(packet) != 12:
                    raise RuntimeError(
                        "Encoded packet is not 12 bytes."
                    )

                client.sendall(packet)
                sent += 1

                expected.append(
                    (
                        sequence,
                        class_index,
                        timestamp_ms,
                    )
                )

                print(
                    f"SENT seq={sequence} "
                    f"class={class_index} "
                    f"timestamp_ms={timestamp_ms} "
                    f"bytes={len(packet)}",
                    flush=True
                )

                time.sleep(0.4)

            time.sleep(4.0)

    except Exception as error:
        test_error = str(error)
        print("TEST ERROR:", test_error, flush=True)

    finally:
        if process.poll() is None:
            process.terminate()

        try:
            qemu_output, _ = process.communicate(
                timeout=5
            )
        except subprocess.TimeoutExpired:
            process.kill()
            qemu_output, _ = process.communicate()

print()
print("=== FREERTOS RECEIVER OUTPUT ===")
print(qemu_output)

print("=== PACKET VERIFICATION ===")

received = []

pattern = re.compile(
    r"FreeRTOS packet seq=(\d+) "
    r"class=(\d+) "
    r"timestamp_ms=(\d+) "
    r"valid=(\d+)"
)

for match in pattern.finditer(qemu_output):
    sequence, class_index, timestamp_ms, valid = (
        map(int, match.groups())
    )

    if valid == 1:
        received.append(
            (sequence, class_index, timestamp_ms)
        )

for item in expected:
    sequence, class_index, timestamp_ms = item

    matched = item in received

    print(
        f"seq={sequence}: "
        f"{'PASS' if matched else 'NOT CONFIRMED'} "
        f"class={class_index} "
        f"timestamp_ms={timestamp_ms}"
    )

print()
print("Packets sent:", sent)
print("Complete valid packets parsed:", len(received))
print("Expected packets verified:", sum(
    item in received for item in expected
))
print(
    "FreeRTOS rejected-packet messages:",
    qemu_output.count("FreeRTOS packet rejected")
)

if (
    test_error is None
    and sent == PACKET_COUNT
    and received == expected
):
    print("MULTI-PACKET GPU TO FREERTOS: PASS")
else:
    print("MULTI-PACKET GPU TO FREERTOS: NOT CONFIRMED")
    raise SystemExit(1)
