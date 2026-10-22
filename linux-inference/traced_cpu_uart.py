import socket
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
import openvino as ov

from packet_codec import encode_packet

project = Path(__file__).resolve().parent.parent
firmware = project / "freertos-qemu/freertos_stack_optimized.elf"
model_path = project / "linux-inference/models/mobilenetv2-12.onnx"

print("=== LIVE MOBILENETV2 CPU INFERENCE ===", flush=True)

core = ov.Core()
model = core.read_model(str(model_path))
compiled = core.compile_model(model, "CPU")

rng = np.random.default_rng(42)
input_data = rng.random((1, 3, 224, 224), dtype=np.float32)

request = compiled.create_infer_request()
outputs = request.infer({compiled.input(0): input_data})
output = np.asarray(outputs[compiled.output(0)])

assert output.shape == (1, 1000)
assert np.isfinite(output).all()

class_index = int(np.argmax(output[0]))
timestamp_ms = (time.monotonic_ns() // 1_000_000) & 0xFFFFFFFF

packet = encode_packet(
    sequence=1,
    class_index=class_index,
    timestamp_ms=timestamp_ms,
    valid=1,
)

print("Execution device:", compiled.get_property("EXECUTION_DEVICES"))
print("Live predicted class:", class_index)
print("Packet size:", len(packet), "bytes")
print("Packet HEX:", packet.hex(" "), flush=True)

with tempfile.TemporaryDirectory(prefix="section23-live-") as folder:
    socket_path = str(Path(folder) / "uart.sock")

    command = [
        "qemu-system-arm",
        "-M", "lm3s6965evb",
        "-display", "none",
        "-monitor", "none",
        "-d", "in_asm,exec,nochain",
        "-dfilter", "0x3ad8..0x3bd6",
        "-D", str(project / "results/decoder_qemu_trace.log"),
        "-kernel", str(firmware),
        "-semihosting-config", "enable=on,target=native",
        "-chardev",
        f"socket,id=uart0,path={socket_path},server=on,wait=off",
        "-serial", "chardev:uart0",
    ]

    print("\n=== STARTING FREERTOS ===", flush=True)

    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    sent = False

    try:
        for _ in range(50):
            if Path(socket_path).exists():
                break
            if process.poll() is not None:
                raise RuntimeError("QEMU exited unexpectedly")
            time.sleep(0.1)
        else:
            raise RuntimeError("UART socket was not created")

        time.sleep(1.0)

        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
            client.settimeout(5)
            client.connect(socket_path)
            client.sendall(packet)
            sent = True
            print("Live inference packet sent: YES", flush=True)
            time.sleep(2.0)

    except Exception as error:
        print("TEST ERROR:", error, flush=True)

    finally:
        process.terminate()

        try:
            qemu_output, _ = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            qemu_output, _ = process.communicate()

        print("\n=== FREERTOS OUTPUT ===")
        print(qemu_output)

        expected = (
            f"FreeRTOS packet seq=1 class={class_index} "
            f"timestamp_ms={timestamp_ms} valid=1"
        )

        if sent and expected in qemu_output:
            print("LIVE CPU INFERENCE TO FREERTOS: PASS")
        else:
            print("LIVE CPU INFERENCE TO FREERTOS: NOT YET CONFIRMED")

        print("QEMU stopped safely")
