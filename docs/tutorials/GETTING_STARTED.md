# Getting Started with NIMCP

This tutorial will guide you through setting up NIMCP and running your first neural network.

## Prerequisites

Before starting, ensure you have:

- **Operating System**: Linux (Ubuntu 20.04+) or macOS (12+)
- **CMake**: 3.16 or later
- **Compiler**: GCC 9+ or Clang 10+
- **Python**: 3.8+ (for Python bindings)

Check your versions:

```bash
cmake --version    # Should be 3.16+
gcc --version      # Should be 9.0+
python3 --version  # Should be 3.8+
```

## Step 1: Clone and Build

```bash
# Clone the repository
git clone https://github.com/redmage123/nimcp.git
cd nimcp

# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake ..

# Build the library
make -j4
```

If the build succeeds, you should see:

```
[100%] Built target nimcp
```

## Step 2: Run the Brain Demo

The simplest way to see NIMCP in action:

```bash
./examples/brain_demo
```

Expected output:

```
=== NIMCP Brain API Demo ===
Creating brain...
Brain created: my_brain (size=1000, task=CLASSIFICATION)
Training on sample data...
Training complete. Accuracy: 92.5%
Making predictions...
Input: [0.5, 0.3, 0.8] -> Output: class_1 (confidence: 0.89)
```

## Step 3: Use Python Bindings

```bash
cd ../bindings/python

# Set library path
export LD_LIBRARY_PATH=../../build/src/lib:$LD_LIBRARY_PATH

# Run the example
python3 example.py
```

Or write your own Python script:

```python
import nimcp

# Create a brain
brain = nimcp.Brain(
    name="my_first_brain",
    size=1000,
    task="classification"
)

# Train on data
for features, label in training_data:
    brain.learn(features, label)

# Make predictions
prediction = brain.decide([0.5, 0.3, 0.8])
print(f"Predicted: {prediction['output']} (confidence: {prediction['confidence']})")
```

## Step 4: Run Tests

Verify your installation by running the test suite:

```bash
cd build
make test
```

All tests should pass:

```
100% tests passed, 0 tests failed out of 25
```

## Next Steps

Now that you have NIMCP running:

1. **Explore the Examples**: See `examples/` directory for more demos
2. **Read the API Reference**: [API_REFERENCE.md](../api/API_REFERENCE.md)
3. **Create Your Own Module**: [CREATE_MODULE.md](CREATE_MODULE.md)
4. **Learn About Brain Regions**: [claude/modules/](../claude/modules/)

## Common Issues

### Library not found

If you see `libnimcp.so not found`:

```bash
export LD_LIBRARY_PATH=/home/bbrelin/nimcp/build/src/lib:$LD_LIBRARY_PATH
```

Add to `~/.bashrc` to make permanent:

```bash
echo 'export LD_LIBRARY_PATH=/home/bbrelin/nimcp/build/src/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

### CMake version too old

Update CMake:

```bash
# Ubuntu
sudo apt update
sudo apt install cmake

# Or install from Kitware repository for latest version
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | sudo apt-key add -
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'
sudo apt update
sudo apt install cmake
```

### Python bindings not working

Ensure Python development headers are installed:

```bash
sudo apt install python3-dev
```

## Getting Help

- **Documentation**: See [docs/INDEX.md](../INDEX.md) for all documentation
- **Issues**: Report bugs at https://github.com/redmage123/nimcp/issues
- **FAQ**: See [FAQ.md](FAQ.md) for common questions
