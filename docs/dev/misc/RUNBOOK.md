# NIMCP Operations Runbook

**Version**: 2.6.3
**Date**: March 2026

This runbook covers building, deploying, operating, monitoring, and troubleshooting NIMCP.

---

## Table of Contents

1. [Environment Setup](#1-environment-setup)
2. [Building](#2-building)
3. [Running](#3-running)
4. [Training Operations](#4-training-operations)
5. [Checkpointing and Recovery](#5-checkpointing-and-recovery)
6. [Monitoring](#6-monitoring)
7. [GPU Operations](#7-gpu-operations)
8. [Memory Management](#8-memory-management)
9. [Common Failure Modes](#9-common-failure-modes)
10. [Performance Tuning](#10-performance-tuning)
11. [Debugging](#11-debugging)
12. [Maintenance Tasks](#12-maintenance-tasks)

---

## 1. Environment Setup

### Required Packages (Ubuntu 22.04+)

```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake gcc g++ \
    python3-dev python3-pip python3-numpy \
    libjansson-dev liblz4-dev libsodium-dev libcurl4-openssl-dev \
    nvidia-cuda-toolkit \
    valgrind gdb
```

### Environment Variables

```bash
# Add to ~/.bashrc or ~/.profile
export NIMCP_HOME=/path/to/nimcp
export PYTHONPATH=$NIMCP_HOME/build:$PYTHONPATH
export LD_LIBRARY_PATH=$NIMCP_HOME/build:$LD_LIBRARY_PATH

# GPU configuration
export CUDA_VISIBLE_DEVICES=0          # Restrict to specific GPU
```

### Verified Hardware

| Component | Tested Configuration |
|-----------|---------------------|
| GPU | NVIDIA RTX 4000 SFF Ada (20 GB VRAM) |
| CUDA | 12.8 |
| Driver | 570+ |
| RAM | 32 GB+ recommended |
| Storage | SSD recommended for checkpoints |

---

## 2. Building

### Standard Build

```bash
cd $NIMCP_HOME
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make nimcp -j$(nproc)
```

**Expected output**: `[100%] Built target nimcp` with zero errors and zero warnings.

**Build time**: ~2-5 minutes on a modern multi-core system.

### Build Targets

| Target | Command | Output |
|--------|---------|--------|
| Core library | `make nimcp -j4` | `libnimcp.so` |
| Python module | `make nimcp_python -j4` | `nimcp.cpython-3XX-x86_64-linux-gnu.so` |
| All tests | `make -j4` | Test binaries |

### Build Variants

```bash
# Debug build with sanitizers
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
make nimcp -j4

# Release with debug symbols (default, recommended)
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
make nimcp -j4

# With code coverage
cmake .. -DENABLE_COVERAGE=ON
make nimcp -j4
```

### Verifying Build

```bash
# Check library exists
ls -la build/libnimcp.so

# Check Python module
python3 -c "import nimcp; print('OK')"

# Run static analysis
make lint           # clang-tidy
make cppcheck       # cppcheck
```

### Clean Rebuild

```bash
cd $NIMCP_HOME/build
rm -rf *
cmake .. -DCMAKE_BUILD_TYPE=Release
make nimcp -j$(nproc)
```

---

## 3. Running

### Python Script Execution

```bash
# Basic training
cd $NIMCP_HOME
python3 scripts/school.py

# Developmental training (Athena)
python3 scripts/immerse_athena.py

# Resume from checkpoint
python3 scripts/immerse_athena.py --resume
```

### C Program Execution

```bash
# Compile against NIMCP
gcc -o my_program my_program.c \
    -I$NIMCP_HOME/include \
    -L$NIMCP_HOME/build \
    -lnimcp -lm -lpthread -latomic

# Run with library path
LD_LIBRARY_PATH=$NIMCP_HOME/build ./my_program
```

### Running Examples

```bash
cd $NIMCP_HOME/build
# Build example
make brain_demo
# Run
./examples/brain_demo
```

---

## 4. Training Operations

### Starting a Training Run

```python
import nimcp

brain = nimcp.Brain(
    name="training_run",
    size=nimcp.BRAIN_LARGE,
    task=nimcp.TASK_CLASSIFICATION,
    num_inputs=1024,
    num_outputs=24,
    init_mode='fast'              # Use FAST init for training
)

brain.enable_checkpointing("checkpoints/run_001")
```

### Monitoring Training Progress

Watch for these indicators:

| Indicator | Healthy | Unhealthy |
|-----------|---------|-----------|
| Loss trend | Decreasing over 100+ examples | Flat or increasing after 1000+ |
| Loss value | < 1.0 for MSE, < 2.0 for CE | > 5.0 or NaN |
| Gradient norm | 0.01 - 5.0 | > 100 or exactly 0 |
| Learning rate (adaptive) | Within [0.1x, 10x] base | At bounds constantly |
| Inference time | Stable | Increasing over time |

### Training Configuration Parameters

| Parameter | Default | Range | Effect |
|-----------|---------|-------|--------|
| Learning rate | 0.001 | 0.0001 - 0.1 | Higher = faster but less stable |
| Gradient clip | 5.0 | 1.0 - 100.0 | Prevents gradient explosion |
| Loss history size | 10 | 5 - 100 | Window for adaptive LR |
| Confidence threshold | 0.9 | 0.5 - 1.0 | Minimum label confidence |

### Stopping Criteria

Training should be stopped when:
- Target accuracy is reached (check evaluation metrics)
- Loss has plateaued for >10,000 examples with no improvement
- NaN appears in loss (indicates numerical instability -- investigate)
- GPU memory is exhausted (OOM)

---

## 5. Checkpointing and Recovery

### Checkpoint Strategy

```python
# Enable at brain creation
brain.enable_checkpointing("/data/checkpoints/experiment_001")

# Checkpoint every N training steps
for i, example in enumerate(training_data):
    brain.learn(example.inputs, example.label, example.confidence)
    if i % 5000 == 0:
        brain.checkpoint()
        print(f"Checkpoint saved at step {i}")
```

### Checkpoint Contents

Each checkpoint saves:
- Neural network weights and biases
- Neuron states (membrane potential, adaptation, spike history)
- Synapse metadata (traces, plasticity state)
- Learning rate and loss history
- Cognitive module states
- Configuration

### Recovering from Checkpoint

```python
brain = nimcp.Brain(
    name="recovered",
    size=nimcp.BRAIN_LARGE,
    task=nimcp.TASK_CLASSIFICATION,
    num_inputs=1024,
    num_outputs=24,
    init_mode='fast'
)
# Load most recent checkpoint
brain.load_checkpoint("/data/checkpoints/experiment_001/latest")
```

### Checkpoint Disk Usage

| Brain Size | Checkpoint Size |
|-----------|----------------|
| 500 neurons | ~5 MB |
| 5,000 neurons | ~50 MB |
| 100,000 neurons | ~500 MB |
| 1,500,000 neurons | ~5 GB |

**Recommendation**: Keep at least 3 recent checkpoints. Rotate older ones to save disk space.

---

## 6. Monitoring

### GPU Monitoring

```bash
# Real-time GPU usage
watch -n 1 nvidia-smi

# Key metrics to watch:
# - GPU Utilization: should be >80% during training
# - Memory Usage: should not approach limit
# - Temperature: should stay below 85C
# - Power Draw: indicates computation intensity
```

### Process Monitoring

```bash
# Memory usage of NIMCP process
ps aux | grep nimcp

# Detailed memory map
pmap -x $(pgrep -f nimcp)

# Thread count
ls /proc/$(pgrep -f nimcp)/task | wc -l
```

### Log Files

Training scripts typically log to:
- `athena.log` -- Athena developmental training
- `immerse_athena.log` -- Immersive training session
- `training_full.log` -- Full training runs

### Health Agent (Programmatic)

```python
brain.health_agent_start()
# Health agent monitors:
# - Memory allocation rate and leaks
# - Thread health and deadlocks
# - Neuron activity distribution
# - Immune system inflammation level
# - GPU memory pressure
```

---

## 7. GPU Operations

### Verifying GPU Availability

```bash
# Check NVIDIA driver
nvidia-smi

# Check CUDA version
nvcc --version

# Check GPU is detected by NIMCP
python3 -c "
import nimcp
brain = nimcp.Brain('test', nimcp.BRAIN_TINY, nimcp.TASK_CLASSIFICATION, 4, 2)
print('Brain created successfully (GPU path)')
"
```

### GPU Memory Management

NIMCP uses `nimcp_malloc/nimcp_free` wrappers for all GPU allocations. Never use raw `malloc` in `.cu` files.

**If GPU memory is exhausted**:
1. Reduce brain size (fewer neurons)
2. Reduce batch size
3. Enable gradient checkpointing (trades compute for memory)
4. Check for memory leaks with `nimcp_memory_check_leaks()`

### Multi-GPU

Multi-GPU support is implemented in `src/gpu/nimcp_multigpu.c` but is not yet validated for production use. Currently, NIMCP operates on a single GPU specified by `CUDA_VISIBLE_DEVICES`.

---

## 8. Memory Management

### Memory Tracking

NIMCP tracks all allocations with canary guards (0xDEADBEEF) for buffer overflow detection.

```c
// Enable detailed tracking
nimcp_memory_enable_tracking(true);

// Check for leaks (call at shutdown)
nimcp_memory_check_leaks();

// Dump allocation details
nimcp_memory_dump_allocations();

// Analyze patterns
nimcp_memory_analyze_patterns();
```

### Memory Budget Guidelines

| Component | Approximate Memory |
|-----------|--------------------|
| Per neuron (sparse) | ~3 KB |
| Per synapse | ~100 bytes |
| Working memory | ~10 KB |
| Immune system | ~1 MB |
| GPU weight cache | Proportional to layer sizes |
| Backprop context | 2x forward pass memory |

### Detecting Memory Issues

**Symptoms of memory leaks**:
- Steadily increasing RSS over time
- OOM errors after extended training
- Increasing checkpoint sizes

**Diagnosis**:
```bash
# Run with AddressSanitizer
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
make nimcp -j4
ASAN_OPTIONS=detect_leaks=1 python3 your_script.py

# Run with Valgrind (slower but more thorough)
valgrind --leak-check=full --suppressions=test/valgrind.supp \
    python3 your_script.py
```

---

## 9. Common Failure Modes

### NaN in Training Loss

**Cause**: Numerical overflow in gradient computation or division by zero.

**Fix**:
1. Check input data for NaN/Inf values
2. Reduce learning rate
3. Verify gradient clipping is enabled (default: 5.0)
4. Check that epsilon clamping is active in loss functions (default: 1e-7)

### Segmentation Fault on Brain Creation

**Cause**: Usually GPU memory exhaustion or invalid configuration.

**Fix**:
1. Try smaller brain size
2. Check `nvidia-smi` for available GPU memory
3. Ensure CUDA drivers are loaded: `nvidia-smi` should succeed
4. Try `init_mode='minimal'` to isolate

### Deadlock During Training

**Cause**: Mutex ordering violation in multi-threaded path.

**Fix**:
1. NIMCP includes a deadlock detector -- check stderr for warnings
2. Ensure you're not calling public mutex-locking functions from within locked code
3. Report as a bug with stack traces

### Immune System Storm

**Cause**: Cascading inflammation from repeated anomalous patterns.

**Symptoms**: Working memory capacity drops to minimum, learning rate near zero, high immune activity in logs.

**Fix**: This is a designed safety mechanism. If it's triggering inappropriately:
1. Check training data for corrupt/adversarial examples
2. Review immune sensitivity thresholds in configuration
3. Consider whether the detected anomaly is genuine

### Slow Initialization

**Cause**: `BRAIN_INIT_FULL` initializes 80+ subsystems.

**Fix**: Use `init_mode='fast'` for training. FAST mode runs 6 of 27 initialization waves and achieves ~14s for 1.5M neurons (vs. 150-250s for FULL).

### Python Import Error

**Cause**: Python module not on path or ABI mismatch.

**Fix**:
```bash
# Verify module exists
find build/ -name "nimcp*.so"

# Set path
export PYTHONPATH=/path/to/nimcp/build:$PYTHONPATH

# Check Python version matches build
python3 --version  # Must match the Python3 CMake found
```

---

## 10. Performance Tuning

### Initialization Speed

| Technique | Speedup | Trade-off |
|-----------|---------|-----------|
| `init_mode='fast'` | 10-20x | Skips cognitive modules |
| `init_mode='minimal'` | 20-50x | Neural network only |
| `parallel_init=True` | 2-4x | More memory during init |
| Reduce neuron count | Linear | Less capacity |

### Training Throughput

| Technique | Effect |
|-----------|--------|
| GPU acceleration | 5-10x vs CPU |
| Larger batch size | Better GPU utilization |
| FAST init | Skip unnecessary subsystems |
| Gradient checkpointing | Reduce memory for deeper nets |
| Sparse synapse storage | 97% memory savings |

### Inference Latency

| Brain Size | CPU Inference | GPU Inference |
|-----------|--------------|---------------|
| 100 neurons | ~0.1 ms | ~0.5 ms (overhead) |
| 1,000 neurons | ~0.5 ms | ~0.3 ms |
| 10,000 neurons | ~5 ms | ~1 ms |
| 100,000 neurons | ~50 ms | ~5 ms |
| 1,000,000 neurons | ~500 ms | ~20 ms |

Note: GPU has higher overhead for small brains due to data transfer. GPU becomes advantageous above ~1,000 neurons.

---

## 11. Debugging

### GDB

```bash
# Build with debug symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug
make nimcp -j4

# Run under GDB
gdb --args python3 your_script.py
(gdb) run
# On crash:
(gdb) bt              # Backtrace
(gdb) info threads     # Thread info
(gdb) thread apply all bt  # All thread backtraces
```

### AddressSanitizer

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
make nimcp -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=0 python3 your_script.py
```

### Valgrind

```bash
valgrind --tool=memcheck \
    --leak-check=full \
    --track-origins=yes \
    --suppressions=test/valgrind.supp \
    --suppressions=test/lsan.supp \
    python3 your_script.py
```

### Debug Logging

NIMCP uses a multi-level logging system:
- `LOG_DEBUG` -- Verbose debugging (disabled in Release)
- `LOG_INFO` -- Normal operation
- `LOG_WARN` -- Warning conditions
- `LOG_ERROR` -- Error conditions
- `LOG_CRITICAL` -- Critical failures

### Core Dump Analysis

```bash
# Enable core dumps
ulimit -c unlimited

# After crash, analyze:
gdb python3 core
(gdb) bt
```

---

## 12. Maintenance Tasks

### Regular Maintenance

| Task | Frequency | Command |
|------|-----------|---------|
| Clean old checkpoints | Weekly | `find checkpoints/ -mtime +30 -delete` |
| Check for memory leaks | After significant changes | Build with ASAN, run tests |
| Rebuild from clean | After CMake changes | `rm -rf build/*; cmake ..; make -j4` |
| Run static analysis | Before commits | `make lint && make cppcheck` |
| Check GPU health | Daily during training | `nvidia-smi -q` |
| Rotate log files | Weekly | `logrotate` or manual |

### Updating NIMCP

```bash
cd $NIMCP_HOME
git pull
cd build
cmake ..             # Re-run CMake to detect changes
make nimcp -j$(nproc)
make nimcp_python -j$(nproc)
```

### Backup Strategy

Critical data to back up:
1. **Checkpoints**: `checkpoints/` directory (largest)
2. **Configuration**: `config/` directory
3. **Training scripts**: `scripts/` directory
4. **Datasets**: `datasets/` directory

### Disk Space Management

```bash
# Check NIMCP disk usage
du -sh $NIMCP_HOME/build/
du -sh $NIMCP_HOME/checkpoints/
du -sh $NIMCP_HOME/datasets/

# Clean build artifacts
cd $NIMCP_HOME/build && make clean

# Remove old checkpoints (keep latest 3)
ls -t checkpoints/*.ckpt | tail -n +4 | xargs rm -f
```

---

*This runbook covers NIMCP v2.6.3 operations. Update this document when procedures change.*
