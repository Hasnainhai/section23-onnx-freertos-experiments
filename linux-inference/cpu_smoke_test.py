import hashlib
from pathlib import Path

import numpy as np
import openvino as ov

model_path = Path("models/mobilenetv2-12.onnx")

print("=== MODEL VERIFICATION ===")
print("Model exists:", model_path.is_file())
print("Model size:", model_path.stat().st_size, "bytes")

print("\n=== OPENVINO CPU INFERENCE ===")

core = ov.Core()
model = core.read_model(str(model_path))
compiled_model = core.compile_model(model, "CPU")

print("Execution device:", compiled_model.get_property("EXECUTION_DEVICES"))
print("Input shape:", compiled_model.input(0).partial_shape)
print("Output shape:", compiled_model.output(0).partial_shape)

# Reproducible synthetic input.
rng = np.random.default_rng(42)
input_data = rng.random((1, 3, 224, 224), dtype=np.float32)

request = compiled_model.create_infer_request()
results = request.infer({compiled_model.input(0): input_data})

output = np.asarray(results[compiled_model.output(0)])

print("\n=== INFERENCE RESULT ===")
print("Output shape:", output.shape)
print("Output dtype:", output.dtype)
print("All values finite:", bool(np.isfinite(output).all()))
print("Predicted class index:", int(np.argmax(output[0])))
print("Highest output score:", float(np.max(output[0])))

assert output.shape == (1, 1000)
assert np.isfinite(output).all()

print("\nCPU INFERENCE TEST: PASS")
