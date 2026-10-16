# Getting Started with NIMCP

Welcome to NIMCP! This guide will help you get up and running with the Neuromorphic Infant Machine Cognitive Platform.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Your First NIMCP Program](#your-first-nimcp-program)
- [Core Concepts](#core-concepts)
- [Common Use Cases](#common-use-cases)
- [Next Steps](#next-steps)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### System Requirements

- **Operating System**: Linux (Ubuntu 20.04+ recommended), macOS, or Windows with WSL2
- **RAM**: 4GB minimum, 8GB+ recommended
- **Storage**: 500MB for base installation, 2GB+ for development

### Required Software

#### Ubuntu/Debian
```bash
# Build essentials
sudo apt-get update
sudo apt-get install -y build-essential cmake git

# Development libraries
sudo apt-get install -y python3-dev libjansson-dev liblz4-dev pkg-config

# Python (if using Python bindings)
sudo apt-get install -y python3 python3-pip python3-venv
```

#### CentOS/RHEL/Fedora
```bash
# Build essentials
sudo yum install -y gcc g++ make cmake git

# Development libraries
sudo yum install -y python3-devel jansson-devel lz4-devel pkgconfig
```

#### macOS
```bash
# Install Homebrew first: https://brew.sh

brew install cmake python jansson lz4
```

### Optional Software

#### GPU Support (NVIDIA CUDA)
```bash
# Ubuntu/Debian
sudo apt-get install -y nvidia-cuda-toolkit

# Check CUDA installation
nvcc --version
```

#### Encryption Support
```bash
# Ubuntu/Debian
sudo apt-get install -y libsodium-dev

# macOS
brew install libsodium
```

## Installation

### Quick Install (Recommended)

```bash
# Clone the repository
git clone https://github.com/yourusername/nimcp.git
cd nimcp

# Run install script
./install.sh
```

The install script will:
1. Check prerequisites
2. Build NIMCP with optimizations
3. Run tests to verify installation
4. Install to `/usr/local` (requires sudo)
5. Set up Python bindings

### Manual Installation

#### 1. Clone Repository

```bash
git clone https://github.com/yourusername/nimcp.git
cd nimcp
```

#### 2. Create Build Directory

```bash
mkdir build
cd build
```

#### 3. Configure Build

```bash
# Release build (recommended for production)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Debug build (for development)
cmake .. -DCMAKE_BUILD_TYPE=Debug

# With all optional features
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=/usr/local
```

#### 4. Build

```bash
# Build with all CPU cores
make -j$(nproc)

# Or specify number of cores
make -j4
```

#### 5. Test

```bash
# Run all tests
ctest -j$(nproc)

# Expected output: 383/383 tests passing
```

#### 6. Install

```bash
# System-wide installation (requires sudo)
sudo make install

# Verify installation
pkg-config --cflags --libs nimcp
```

### Python Setup

If you installed system-wide, Python bindings should work automatically:

```bash
python3 -c "import nimcp; print(nimcp.__version__)"
# Output: 2.6.2
```

If not, set your Python path:

```bash
export PYTHONPATH=/usr/local/lib/python3/dist-packages:$PYTHONPATH
```

Or for development:

```bash
export PYTHONPATH=/path/to/nimcp/build/lib/python:$PYTHONPATH
```

### Verify Installation

```bash
# Check library
ls -la /usr/local/lib/libnimcp.so*

# Check headers
ls /usr/local/include/nimcp/

# Check pkg-config
pkg-config --cflags --libs nimcp

# Run example
cd /path/to/nimcp
./build/bin/simple_demo
```

## Your First NIMCP Program

### Hello World (C)

Create `hello_nimcp.c`:

```c
#include <stdio.h>
#include <nimcp.h>

int main(void) {
    printf("NIMCP Version: %s\n", NIMCP_VERSION_STRING);

    // Create a tiny brain
    nimcp_brain_t brain = nimcp_brain_create(
        "hello_brain",
        NIMCP_BRAIN_TINY,               // 100 neurons
        NIMCP_BRAIN_TASK_CLASSIFICATION,
        4,                              // 4 inputs
        2                               // 2 outputs
    );

    if (!brain) {
        fprintf(stderr, "Failed to create brain\n");
        return 1;
    }

    printf("Brain created successfully!\n");
    printf("Brain has %u neurons\n", nimcp_brain_get_neuron_count(brain));

    // Clean up
    nimcp_brain_destroy(brain);

    return 0;
}
```

Compile and run:

```bash
# Using pkg-config
gcc hello_nimcp.c -o hello_nimcp $(pkg-config --cflags --libs nimcp)

# Or manually
gcc hello_nimcp.c -o hello_nimcp -I/usr/local/include -L/usr/local/lib -lnimcp

# Run
./hello_nimcp
```

### Hello World (Python)

Create `hello_nimcp.py`:

```python
#!/usr/bin/env python3
import nimcp

print(f"NIMCP Version: {nimcp.__version__}")

# Create a tiny brain
brain = nimcp.Brain(
    name="hello_brain",
    size=nimcp.BrainSize.TINY,          # 100 neurons
    task=nimcp.BrainTask.CLASSIFICATION,
    num_inputs=4,
    num_outputs=2
)

print("Brain created successfully!")
print(f"Brain has {brain.get_neuron_count()} neurons")
```

Run:

```bash
python3 hello_nimcp.py
```

## Core Concepts

### 1. Brain Creation

The brain is the top-level abstraction in NIMCP:

```c
nimcp_brain_t brain = nimcp_brain_create(
    const char* name,           // Human-readable name
    nimcp_brain_size_t size,    // TINY/SMALL/MEDIUM/LARGE
    nimcp_brain_task_t task,    // CLASSIFICATION/REGRESSION/etc
    uint32_t num_inputs,        // Number of input features
    uint32_t num_outputs        // Number of output classes
);
```

**Brain Sizes:**

| Size   | Neurons | Memory  | Inference | Use Case                    |
|--------|---------|---------|-----------|----------------------------|
| TINY   | 100     | <1 MB   | ~0.1 ms   | IoT devices, testing       |
| SMALL  | 1,000   | ~10 MB  | ~0.5 ms   | Mobile apps, edge devices  |
| MEDIUM | 10,000  | ~50 MB  | ~5 ms     | Desktop apps, servers      |
| LARGE  | 100,000 | ~500 MB | ~50 ms    | High-performance servers   |

**Task Types:**

```c
NIMCP_BRAIN_TASK_CLASSIFICATION  // Classify inputs into categories
NIMCP_BRAIN_TASK_REGRESSION      // Predict continuous values
NIMCP_BRAIN_TASK_REINFORCEMENT   // Learn from rewards
NIMCP_BRAIN_TASK_AUTOENCODER     // Learn compressed representations
```

### 2. Learning

Train the brain with examples:

```c
// Single example
float inputs[4] = {0.5, 0.8, 0.2, 0.9};
float targets[2] = {1.0, 0.0};  // Class 0

float loss = nimcp_brain_learn_example(brain, inputs, 4, targets, 2);
printf("Loss: %.4f\n", loss);

// Batch learning (more efficient)
float batch_inputs[10][4] = { /* ... */ };
float batch_targets[10][2] = { /* ... */ };

float avg_loss = nimcp_brain_learn_batch(
    brain,
    (float*)batch_inputs, 4,
    (float*)batch_targets, 2,
    10  // batch size
);
```

**Python:**

```python
import numpy as np

# Single example
inputs = np.array([0.5, 0.8, 0.2, 0.9])
targets = np.array([1.0, 0.0])

loss = brain.learn_example(inputs, targets)
print(f"Loss: {loss:.4f}")

# Batch learning
batch_inputs = np.random.rand(10, 4)
batch_targets = np.eye(2)[np.random.randint(0, 2, 10)]

avg_loss = brain.learn_batch(batch_inputs, batch_targets)
```

### 3. Prediction

Make predictions on new data:

```c
float inputs[4] = {0.6, 0.7, 0.3, 0.8};
float outputs[2];

nimcp_status_t status = nimcp_brain_predict(brain, inputs, 4, outputs, 2);
if (status == NIMCP_STATUS_SUCCESS) {
    printf("Prediction: [%.3f, %.3f]\n", outputs[0], outputs[1]);

    // Get predicted class
    int predicted_class = (outputs[0] > outputs[1]) ? 0 : 1;
    printf("Predicted class: %d\n", predicted_class);
}
```

**Python:**

```python
inputs = np.array([0.6, 0.7, 0.3, 0.8])
outputs = brain.predict(inputs)

print(f"Prediction: {outputs}")
print(f"Predicted class: {np.argmax(outputs)}")
```

### 4. Persistence (Save/Load)

Save and restore brain state:

```c
// Save brain
nimcp_status_t status = nimcp_brain_save_snapshot(
    brain,
    "my_brain_checkpoint",  // snapshot name
    "/path/to/snapshots"    // directory
);

// Load brain
nimcp_brain_t loaded_brain = nimcp_brain_restore_snapshot(
    "my_brain_checkpoint",
    "/path/to/snapshots"
);
```

**Python:**

```python
# Save brain
brain.save_snapshot("my_brain_checkpoint", "/path/to/snapshots")

# Load brain
loaded_brain = nimcp.Brain.restore_snapshot(
    "my_brain_checkpoint",
    "/path/to/snapshots"
)
```

## Common Use Cases

### Image Classification

```c
#include <nimcp.h>
#include <stdio.h>

int main(void) {
    // Create brain for 28x28 grayscale images, 10 classes
    nimcp_brain_t brain = nimcp_brain_create(
        "digit_classifier",
        NIMCP_BRAIN_MEDIUM,
        NIMCP_BRAIN_TASK_CLASSIFICATION,
        28 * 28,  // 784 pixels
        10        // digits 0-9
    );

    // Training loop
    for (int epoch = 0; epoch < 10; epoch++) {
        float total_loss = 0.0f;
        int num_examples = 1000;

        for (int i = 0; i < num_examples; i++) {
            float* image = load_image(i);      // Your function
            float* label = load_label(i);      // Your function

            float loss = nimcp_brain_learn_example(
                brain, image, 784, label, 10
            );
            total_loss += loss;

            free(image);
            free(label);
        }

        printf("Epoch %d: Loss = %.4f\n", epoch, total_loss / num_examples);
    }

    // Test
    float* test_image = load_image(1001);
    float outputs[10];
    nimcp_brain_predict(brain, test_image, 784, outputs, 10);

    // Find max output
    int predicted = 0;
    float max_val = outputs[0];
    for (int i = 1; i < 10; i++) {
        if (outputs[i] > max_val) {
            max_val = outputs[i];
            predicted = i;
        }
    }

    printf("Predicted digit: %d (confidence: %.2f%%)\n",
           predicted, max_val * 100);

    free(test_image);
    nimcp_brain_destroy(brain);
    return 0;
}
```

### Regression

```c
// Predict house prices from features
nimcp_brain_t brain = nimcp_brain_create(
    "house_price_predictor",
    NIMCP_BRAIN_SMALL,
    NIMCP_BRAIN_TASK_REGRESSION,
    5,  // 5 features: size, bedrooms, bathrooms, location, age
    1   // 1 output: price
);

// Training data
float features[5] = {2000, 3, 2, 0.8, 10};  // sq ft, beds, baths, location, age
float price[1] = {350000};  // price in dollars (normalized)

nimcp_brain_learn_example(brain, features, 5, price, 1);

// Prediction
float new_house[5] = {2500, 4, 3, 0.9, 5};
float predicted_price[1];
nimcp_brain_predict(brain, new_house, 5, predicted_price, 1);

printf("Predicted price: $%.0f\n", predicted_price[0]);
```

### Multi-Modal Processing

Process vision, audio, and language together:

```c
#include <core/brain/nimcp_brain.h>

// Enable multimodal features
brain_config_t config = {0};
config.size = BRAIN_SIZE_MEDIUM;
config.task = BRAIN_TASK_CLASSIFICATION;
config.num_inputs = 0;  // Not used for multimodal
config.num_outputs = 3;

// Enable modalities
config.enable_visual_cortex = true;
config.enable_audio_cortex = true;
config.enable_speech_cortex = true;
config.enable_multimodal_integration = true;

brain_t brain = brain_create_custom(&config);

// Prepare multimodal input
brain_multimodal_input_t input = {0};

// Visual data (8x8 grayscale image)
uint8_t image[64] = { /* image pixels */ };
input.visual_data = image;
input.visual_width = 8;
input.visual_height = 8;
input.visual_channels = 1;

// Audio data (PCM samples)
float audio[1024] = { /* audio samples */ };
input.audio_data = audio;
input.audio_samples = 1024;
input.audio_channels = 1;

// Language data
input.language_text = "What is this?";
input.language_length = 13;

// Process
brain_multimodal_output_t output = {0};
brain_process_multimodal(brain, &input, &output);

printf("Decision: %s\n", output.decision_label);
printf("Confidence: %.2f%%\n", output.confidence * 100);
printf("Visual attention: %.2f%%\n", output.visual_attention * 100);
printf("Audio attention: %.2f%%\n", output.audio_attention * 100);
printf("Language attention: %.2f%%\n", output.language_attention * 100);
```

## Next Steps

### Explore Examples

Check out the comprehensive examples:

```bash
cd examples

# Basic examples
./build/bin/simple_demo          # Simple classification
./build/bin/brain_demo           # Brain API showcase
./build/bin/ethics_demo          # Ethical reasoning

# Advanced examples
./build/bin/multimodal_integration_demo   # Vision + Audio + Language
./build/bin/working_memory_public_api_demo  # Working memory
./build/bin/distributed_cow_demo  # Distributed deployment

# Python examples
python3 simple_demo.py
python3 simple_web_demo.py       # Web interface
```

### Read Documentation

- **[Architecture Summary](ARCHITECTURE_SUMMARY.md)** - System internals
- **[API Reference](api/API_REFERENCE.md)** - Complete API docs
- **[Cognitive Quick Reference](COGNITIVE_QUICK_REFERENCE.md)** - Cognitive features
- **[Build Quick Reference](BUILD_QUICK_REFERENCE.md)** - Build system

### Learn Advanced Features

- **[Copy-on-Write Cloning](DISTRIBUTED_COW_README.md)** - Efficient brain cloning
- **[Ethical Guidelines](ETHICAL_GUIDELINES.md)** - Responsible AI
- **[Security](SECURITY.md)** - Security best practices
- **[Training Guide](PROGRESSIVE_TRAINING_GUIDE.md)** - Advanced training

### Join the Community

- **GitHub**: [https://github.com/yourusername/nimcp](https://github.com/yourusername/nimcp)
- **Issues**: Report bugs or request features
- **Discussions**: Ask questions and share ideas
- **Contributing**: See [CONTRIBUTING.md](../CONTRIBUTING.md)

## Troubleshooting

### Installation Issues

#### "Python3 not found"
```bash
# Install Python development headers
sudo apt-get install python3-dev

# Or specify Python path
cmake .. -DPython3_ROOT_DIR=/usr/bin/python3
```

#### "GTest not found"
```bash
# Install GTest
sudo apt-get install libgtest-dev

# Or let CMake fetch it automatically (default)
```

#### "CUDA not found"
```bash
# Install CUDA toolkit
sudo apt-get install nvidia-cuda-toolkit

# Or build without CUDA (automatic fallback)
```

#### "libjansson not found"
```bash
sudo apt-get install libjansson-dev
```

### Runtime Issues

#### "libnimcp.so: cannot open shared object file"
```bash
# Set library path
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Or add to system path
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/nimcp.conf
sudo ldconfig
```

#### "Python import nimcp fails"
```bash
# Set Python path
export PYTHONPATH=/usr/local/lib/python3/dist-packages:$PYTHONPATH

# Or for development
export PYTHONPATH=/path/to/nimcp/build/lib/python:$PYTHONPATH
```

#### Out of Memory
```bash
# Use smaller brain size
# TINY (100 neurons) instead of LARGE (100,000 neurons)

# Or increase system swap
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

### Build Issues

#### Compiler warnings
```bash
# Ensure you have recent compiler
gcc --version  # Should be 7.0+
g++ --version  # Should be 7.0+

# Update if needed
sudo apt-get install gcc-11 g++-11
```

#### CMake version too old
```bash
# Install newer CMake
sudo apt-get install cmake

# Or from source
wget https://github.com/Kitware/CMake/releases/download/v3.25.0/cmake-3.25.0.tar.gz
tar -xzf cmake-3.25.0.tar.gz
cd cmake-3.25.0
./bootstrap && make && sudo make install
```

### Getting Help

If you're still stuck:

1. Check [documentation](../docs/)
2. Search [existing issues](https://github.com/yourusername/nimcp/issues)
3. Ask in [GitHub Discussions](https://github.com/yourusername/nimcp/discussions)
4. Email: braun.brelin@ai-elevate.ai

---

**You're ready to build brain-inspired AI with NIMCP!**
