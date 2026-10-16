# NIMCP User Guide

**Version**: 2.6.3
**Date**: March 2026

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Installation](#2-installation)
3. [Quick Start](#3-quick-start)
4. [Core Concepts](#4-core-concepts)
5. [Creating a Brain](#5-creating-a-brain)
6. [Training](#6-training)
7. [Inference (Prediction)](#7-inference-prediction)
8. [Python API](#8-python-api)
9. [C API](#9-c-api)
10. [Configuration](#10-configuration)
11. [Checkpointing and Persistence](#11-checkpointing-and-persistence)
12. [GPU Acceleration](#12-gpu-acceleration)
13. [Developmental Training (Athena)](#13-developmental-training-athena)
14. [Monitoring and Introspection](#14-monitoring-and-introspection)
15. [Advanced Features](#15-advanced-features)
16. [Troubleshooting](#16-troubleshooting)

---

## 1. Introduction

NIMCP is a neuromorphic computing platform that creates brain-like neural networks for classification, regression, and pattern recognition tasks. It uses biologically-inspired mechanisms including spiking neurons, diverse synapse types, and cognitive modules.

### Who This Guide Is For

- Researchers exploring neuromorphic approaches to AI
- Developers integrating NIMCP into applications
- Anyone evaluating NIMCP's capabilities

### Prerequisites

- Linux system (Ubuntu 20.04+ recommended)
- C11-compatible compiler (GCC 7+ or Clang 6+)
- CMake 3.15+
- Python 3.7+ (for Python bindings)
- Optional: NVIDIA GPU with CUDA 11.0+ (strongly recommended)

---

## 2. Installation

### Dependencies

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake python3-dev libjansson-dev liblz4-dev

# Optional: GPU support
sudo apt-get install nvidia-cuda-toolkit

# Optional: encryption support
sudo apt-get install libsodium-dev
```

### Building from Source

```bash
git clone https://github.com/redmage123/nimcp.git
cd nimcp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make nimcp -j$(nproc)
```

### Building Python Bindings

```bash
# From the build directory
make nimcp_python -j$(nproc)
```

The Python module will be built as `nimcp.cpython-3XX-x86_64-linux-gnu.so` in the `build/` directory. Add this directory to your `PYTHONPATH`:

```bash
export PYTHONPATH=/path/to/nimcp/build:$PYTHONPATH
```

### Verifying Installation

```python
import nimcp
print(nimcp.__version__)  # Should print version string
```

---

## 3. Quick Start

### Python (Recommended)

```python
import nimcp

# Create a small brain for classification
brain = nimcp.Brain(
    name="quickstart",
    size=nimcp.BRAIN_SMALL,           # 500 neurons
    task=nimcp.TASK_CLASSIFICATION,
    num_inputs=4,
    num_outputs=3
)

# Train on some examples
brain.learn([5.1, 3.5, 1.4, 0.2], "setosa", 0.9)
brain.learn([7.0, 3.2, 4.7, 1.4], "versicolor", 0.9)
brain.learn([6.3, 3.3, 6.0, 2.5], "virginica", 0.9)

# Predict
label, confidence = brain.predict([5.0, 3.4, 1.5, 0.2])
print(f"Predicted: {label} (confidence: {confidence:.2f})")
```

### C

```c
#include <nimcp.h>

int main(void) {
    nimcp_brain_t brain = nimcp_brain_create(
        "quickstart",
        NIMCP_BRAIN_SMALL,
        NIMCP_BRAIN_TASK_CLASSIFICATION,
        4, 3
    );

    float inputs[] = {5.1, 3.5, 1.4, 0.2};
    float target[] = {1.0, 0.0, 0.0};

    float loss = nimcp_brain_learn_example(brain, inputs, 4, target, 3);
    printf("Loss: %.4f\n", loss);

    float outputs[3];
    nimcp_brain_predict(brain, inputs, 4, outputs, 3);
    printf("Output: [%.2f, %.2f, %.2f]\n", outputs[0], outputs[1], outputs[2]);

    nimcp_brain_destroy(brain);
    return 0;
}
```

Compile with:
```bash
gcc -o quickstart quickstart.c -lnimcp -L/path/to/nimcp/build -I/path/to/nimcp/include
```

---

## 4. Core Concepts

### Brain

The `Brain` is the top-level object. It encapsulates the neural network, cognitive modules, and all subsystems. You create a brain, train it, and use it for inference.

### Brain Sizes

| Size | Neurons | Memory | Typical Use |
|------|---------|--------|-------------|
| `BRAIN_MICRO` | 25 | <1 MB | Testing |
| `BRAIN_TINY` | 100 | ~1 MB | Toy problems |
| `BRAIN_SMALL` | 500 | ~10 MB | Simple classification |
| `BRAIN_MEDIUM` | 1,000 | ~50 MB | Standard tasks |
| `BRAIN_LARGE` | 5,000 | ~200 MB | Complex tasks |
| `BRAIN_CUSTOM` | User-defined | Varies | Research |

### Task Types

| Task | Description |
|------|-------------|
| `TASK_CLASSIFICATION` | Discrete category prediction |
| `TASK_REGRESSION` | Continuous value prediction |
| `TASK_PATTERN_MATCHING` | Pattern recognition |
| `TASK_SEQUENCE` | Sequence processing |
| `TASK_ASSOCIATION` | Associative learning |

### Initialization Modes

| Mode | Speed | Subsystems | Use When |
|------|-------|-----------|----------|
| `'full'` (default) | Slowest | All 80+ | Full-featured research |
| `'fast'` | ~10x faster | Core + GPU + training | Training iteration |
| `'minimal'` | Fastest | Neural network only | Benchmarking |

---

## 5. Creating a Brain

### Python

```python
import nimcp

# Standard creation
brain = nimcp.Brain(
    name="my_brain",
    size=nimcp.BRAIN_MEDIUM,
    task=nimcp.TASK_CLASSIFICATION,
    num_inputs=10,
    num_outputs=5
)

# Fast initialization (recommended for training)
brain = nimcp.Brain(
    name="fast_brain",
    size=nimcp.BRAIN_LARGE,
    task=nimcp.TASK_CLASSIFICATION,
    num_inputs=100,
    num_outputs=10,
    init_mode='fast'
)
```

### C

```c
// Standard creation
nimcp_brain_t brain = nimcp_brain_create(
    "my_brain",
    NIMCP_BRAIN_MEDIUM,
    NIMCP_BRAIN_TASK_CLASSIFICATION,
    10, 5
);

// Always destroy when done
nimcp_brain_destroy(brain);
```

### Custom Neuron Counts

For research use cases requiring specific neuron counts:

```python
brain = nimcp.Brain(
    name="research_brain",
    size=nimcp.BRAIN_CUSTOM,
    task=nimcp.TASK_CLASSIFICATION,
    num_inputs=1024,
    num_outputs=24,
    num_neurons=1500000,    # 1.5M neurons
    init_mode='fast'        # Recommended for large brains
)
```

---

## 6. Training

### Single Example Training

```python
# Classification: provide input vector, label, and confidence
loss = brain.learn(
    inputs=[1.0, 0.5, 0.3, 0.8],
    label="category_a",
    confidence=0.9                   # How confident you are in this label
)
print(f"Training loss: {loss:.4f}")
```

### Batch Training

```python
import numpy as np

# Prepare batch data
inputs_batch = np.array([
    [1.0, 0.5, 0.3, 0.8],
    [0.2, 0.9, 0.1, 0.4],
    [0.7, 0.3, 0.8, 0.2],
])
labels = ["cat_a", "cat_b", "cat_a"]
confidences = [0.9, 0.85, 0.95]

# Train on batch
for inp, lbl, conf in zip(inputs_batch, labels, confidences):
    loss = brain.learn(inp.tolist(), lbl, conf)
```

### Vector-Based Training (Regression)

```python
# For regression or custom targets
loss = brain.learn_vector(
    inputs=[1.0, 2.0, 3.0],
    targets=[0.5, 0.8]
)
```

### Training Tips

1. **Start with FAST init** for iterative training: `init_mode='fast'`
2. **Monitor loss**: If loss plateaus, the adaptive learning rate will adjust automatically
3. **Provide high-confidence examples first**: This helps the network establish stable initial weights
4. **Use checkpointing** for long training runs (see Section 11)
5. **Gradient clipping** is enabled by default at 5.0 to prevent exploding gradients

---

## 7. Inference (Prediction)

### Classification

```python
# Returns (label, confidence)
label, confidence = brain.predict([1.0, 0.5, 0.3, 0.8])
print(f"Predicted: {label} with confidence {confidence:.2%}")
```

### Getting Raw Output Vector

```python
# Get the full output vector
decision = brain.decide([1.0, 0.5, 0.3, 0.8])
print(f"Label: {decision.label}")
print(f"Confidence: {decision.confidence}")
print(f"Output vector: {decision.output_vector}")
print(f"Active neurons: {decision.num_active_neurons}")
print(f"Inference time: {decision.inference_time_us} us")
```

### Batch Prediction

```python
inputs_batch = [
    [1.0, 0.5, 0.3, 0.8],
    [0.2, 0.9, 0.1, 0.4],
]
labels, confidences = brain.predict_batch(inputs_batch)
for label, conf in zip(labels, confidences):
    print(f"{label}: {conf:.2%}")
```

---

## 8. Python API

### Brain Class Methods

| Method | Description |
|--------|-------------|
| `Brain(name, size, task, num_inputs, num_outputs, **kwargs)` | Create a brain |
| `learn(inputs, label, confidence)` | Train on labeled example |
| `learn_vector(inputs, targets)` | Train on input/target vectors |
| `predict(inputs)` | Returns (label, confidence) |
| `predict_batch(inputs_list)` | Batch prediction |
| `decide(inputs)` | Full decision with metadata |
| `show_and_name(name, description)` | Association learning |
| `decide_full(features)` | Full pipeline with ethics/immune |
| `enable_checkpointing(path)` | Enable checkpoints |
| `checkpoint()` | Save checkpoint |
| `list_checkpoints()` | List saved checkpoints |
| `health_agent_start()` | Start health monitoring |

### Brain Properties

| Property | Description |
|----------|-------------|
| `name` | Brain name |
| `num_neurons` | Total neuron count |
| `num_inputs` | Input dimension |
| `num_outputs` | Output dimension |

### Module Constants

```python
# Brain sizes
nimcp.BRAIN_MICRO, nimcp.BRAIN_TINY, nimcp.BRAIN_SMALL
nimcp.BRAIN_MEDIUM, nimcp.BRAIN_LARGE, nimcp.BRAIN_CUSTOM

# Task types
nimcp.TASK_CLASSIFICATION, nimcp.TASK_REGRESSION
nimcp.TASK_PATTERN_MATCHING, nimcp.TASK_SEQUENCE
nimcp.TASK_ASSOCIATION
```

---

## 9. C API

### Core Functions

```c
// Lifecycle
nimcp_brain_t nimcp_brain_create(const char* name, nimcp_brain_size_t size,
                                  nimcp_brain_task_t task,
                                  uint32_t num_inputs, uint32_t num_outputs);
void nimcp_brain_destroy(nimcp_brain_t brain);

// Training
float nimcp_brain_learn_example(nimcp_brain_t brain,
                                 const float* inputs, uint32_t num_inputs,
                                 const float* targets, uint32_t num_outputs);

// Inference
nimcp_status_t nimcp_brain_predict(nimcp_brain_t brain,
                                    const float* inputs, uint32_t num_inputs,
                                    float* outputs, uint32_t num_outputs);

// Decision (full pipeline)
brain_decision_t* brain_decide(brain_t brain,
                                const float* features, uint32_t num_features);
void brain_free_decision(brain_decision_t* decision);
```

### Error Handling

```c
nimcp_status_t status = nimcp_brain_predict(brain, inputs, 4, outputs, 3);
if (status != NIMCP_OK) {
    fprintf(stderr, "Prediction failed: %d\n", status);
}
```

### Memory Management

- Always call `nimcp_brain_destroy()` when done with a brain
- Always call `brain_free_decision()` on decision results
- Never use `nimcp_free()` on decision structs (use `brain_free_decision`)
- Use `copy_decision_deep()` if you need to cache a decision

---

## 10. Configuration

### Runtime Configuration

Configuration files are in `config/`:

**`nimcp_default.conf`**:
```ini
[learning]
learning_rate = 0.001
batch_size = 32
dropout_rate = 0.5

[stdp]
stdp_window_ms = 20
stdp_a_plus = 0.01
stdp_tau_plus_ms = 20.0

[neuromodulators]
dopamine_baseline = 0.2
serotonin_baseline = 0.5
acetylcholine_baseline = 0.3

[features]
enable_cow = true
enable_cache = true
enable_working_memory = true
enable_sleep_wake_cycle = true
```

### Programmatic Configuration

In Python, configuration is passed through keyword arguments to `Brain()`:

```python
brain = nimcp.Brain(
    name="configured_brain",
    size=nimcp.BRAIN_MEDIUM,
    task=nimcp.TASK_CLASSIFICATION,
    num_inputs=10,
    num_outputs=5,
    init_mode='fast',          # Initialization mode
    # Additional config through brain_config_t fields
)
```

---

## 11. Checkpointing and Persistence

### Enabling Checkpoints

```python
brain = nimcp.Brain("my_brain", nimcp.BRAIN_MEDIUM,
                    nimcp.TASK_CLASSIFICATION, 10, 5)

# Enable checkpointing to a directory
brain.enable_checkpointing("/path/to/checkpoints")

# Train for a while...
for example in training_data:
    brain.learn(example.inputs, example.label, example.confidence)

# Save a checkpoint
brain.checkpoint()
```

### Listing Checkpoints

```python
checkpoints = brain.list_checkpoints()
for cp in checkpoints:
    print(cp)
```

### Checkpoint Contents

Checkpoints save:
- Full neural network state (neuron states, synapse weights)
- Cognitive module states
- Training progress (loss history, learning rate)
- Configuration

---

## 12. GPU Acceleration

NIMCP follows a **GPU-first policy**. If a CUDA-capable GPU is available, operations automatically use the GPU.

### Requirements

- NVIDIA GPU with CUDA compute capability 5.0+
- CUDA Toolkit 11.0+
- NIMCP built with CUDA support (auto-detected by CMake)

### Verifying GPU Usage

```python
import nimcp
# GPU is used automatically when available
brain = nimcp.Brain("gpu_brain", nimcp.BRAIN_LARGE,
                    nimcp.TASK_CLASSIFICATION, 100, 10)
# Forward and backward passes run on GPU
```

### CPU Fallback

If CUDA is not available, NIMCP automatically uses CPU implementations from `src/gpu/stubs/`. No code changes are required.

### Memory Considerations

| Brain Size | GPU Memory |
|-----------|-----------|
| 1K neurons | ~10 MB |
| 10K neurons | ~100 MB |
| 100K neurons | ~1 GB |
| 1M neurons | ~10 GB |
| 1.5M neurons | ~15 GB |

The RTX 4000 SFF Ada (20 GB) supports up to ~1.5M neurons comfortably.

---

## 13. Developmental Training (Athena)

The developmental training system trains a brain through biologically-inspired stages.

### Running Athena

```bash
cd /path/to/nimcp
python scripts/immerse_athena.py
```

### Stages

| Stage | Stimuli | Goal |
|-------|---------|------|
| 0: Sensory Awakening | 10,000 | Self-reconstruction of sensory patterns |
| 1: Association | 20,000 | Cross-modal binding (name + description) |
| 2: Conceptual Learning | 30,000 | Category formation, abstract reasoning |
| 3: Reasoning & Dialogue | Unlimited | Complex reasoning, ethical judgment |

### Resuming Training

Athena saves checkpoints every 5,000 stimuli. To resume:

```bash
python scripts/immerse_athena.py --resume
```

### Monitoring Progress

The training script logs:
- Loss per stimulus
- Accuracy at evaluation checkpoints (every 500 stimuli)
- Memory consolidation events
- Stage transitions

---

## 14. Monitoring and Introspection

### Health Agent

```python
brain.health_agent_start()
# Health agent monitors:
# - Memory usage
# - Thread health
# - Neuron activity statistics
# - Immune system status
```

### Introspection

```python
# Get active neuron population
decision = brain.decide(inputs)
print(f"Active neurons: {decision.num_active_neurons}")
print(f"Sparsity: {decision.sparsity:.2%}")
print(f"Explanation: {decision.explanation}")
```

### Immune System Status

The immune system runs automatically, monitoring for anomalous activation patterns. During high inflammation:
- Working memory capacity decreases
- Learning rate is suppressed
- Theory of Mind processing is impaired

---

## 15. Advanced Features

### Copy-on-Write Cloning

Create lightweight brain copies that share memory until modified:

```c
nimcp_brain_t clone = nimcp_brain_clone_cow(original);
// Clone uses ~1MB vs ~50MB for full copy
// Modifications trigger copy-on-write
```

### Multi-Modal Processing

```c
brain_multimodal_input_t input = {
    .visual_data = image_pixels,
    .visual_width = 8, .visual_height = 8,
    .audio_data = audio_samples,
    .audio_samples = 1024,
    .language_text = "What do you see?"
};
brain_multimodal_output_t output;
brain_process_multimodal(brain, &input, &output);
```

### Custom Neuron Types

NIMCP supports 30+ specialized neuron types including visual cortex cells, auditory neurons, motor neurons, cognitive neurons, and neural logic gates. See `include/core/neuron_types/nimcp_neuron_types.h` for the complete list.

---

## 16. Troubleshooting

### Build Issues

**CMake can't find Python**:
```bash
cmake .. -DPython3_EXECUTABLE=/usr/bin/python3
```

**CUDA not detected**:
```bash
cmake .. -DCUDA_TOOLKIT_ROOT_DIR=/usr/local/cuda
```

**Missing libjansson**:
```bash
sudo apt-get install libjansson-dev
```

### Runtime Issues

**Segfault on brain creation**: Ensure you're not exceeding available GPU memory. Try `BRAIN_SMALL` first.

**Slow initialization**: Use `init_mode='fast'` for training. Full init is only needed when all cognitive modules are required.

**NaN in training loss**: This is usually caused by:
- Learning rate too high (the adaptive system will auto-correct)
- Input data not normalized (scale inputs to [0, 1] or [-1, 1])
- Division by zero in custom loss (epsilon clamping prevents this in built-in losses)

**Import error for Python module**: Ensure the build directory is in your `PYTHONPATH` and the `.so` file exists:
```bash
ls build/*.cpython*.so
export PYTHONPATH=/path/to/nimcp/build:$PYTHONPATH
```

### Getting Help

- Documentation: `docs/EXTERNAL_API_GUIDE.md`
- Issues: https://github.com/redmage123/nimcp/issues
- Contact: braun.brelin@ai-elevate.ai

---

*This guide covers NIMCP v2.6.3. The system is under active development.*
