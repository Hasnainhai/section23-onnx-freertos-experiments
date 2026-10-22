from pathlib import Path

import numpy as np
import openvino as ov

model_path = Path("models/mobilenetv2-12.onnx")

print("=== GPU INFERENCE TEST ===")
print("Model exists:", model_path.is_file())

core = ov.Core()

print("Available devices:", core.available_devices)

print("\n=== COMPILING MODEL FOR GPU ===")

model = core.read_model(str(model_path))
compiled_model = core.compile_model(model, "GPU")

print(
    "Execution device:",
    compiled_model.get_property("EXECUTION_DEVICES")
)

print("\n=== RUNNING GPU INFERENCE ===")

rng = np.random.default_rng(42)
input_data = rng.random((1, 3, 224, 224), dtype=np.float32)

request = compiled_model.create_infer_request()

results = request.infer({
    compiled_model.input(0): input_data
})

output = np.asarray(
    results[compiled_model.output(0)]
)

print("Output shape:", output.shape)
print("Output dtype:", output.dtype)
print("All values finite:", bool(np.isfinite(output).all()))
print("Predicted class index:", int(np.argmax(output[0])))
print("Highest output score:", float(np.max(output[0])))

assert output.shape == (1, 1000)
assert np.isfinite(output).all()

print("\nGPU INFERENCE TEST: PASS")
