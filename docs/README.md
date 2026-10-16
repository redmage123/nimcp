# NIMCP - Neuromorphic Infant Machine Cognitive Platform

[![Version](https://img.shields.io/badge/version-0.9.0--beta-blue.svg)](https://github.com/redmage123/nimcp)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))

A biologically-inspired neural computing framework with advanced cognitive capabilities, combining spiking neural networks with 30+ cognitive modules to create brain-like artificial intelligence.

## Overview

NIMCP is a sophisticated neural framework designed to mimic biological brain function through:

- **Biologically Realistic Networks**: Spiking neurons with STDP learning, diverse neurotransmitter systems, and brain oscillations
- **Cognitive Architecture**: 30+ modules including working memory, ethics, theory of mind, and emotional intelligence
- **Multi-Modal Processing**: Unified processing of vision, audio, language, and structured data
- **Distributed Computing**: P2P networking with efficient copy-on-write cloning (86% memory savings)
- **Multiple Language Bindings**: Python (primary), Go, Rust, Java, Node.js, Ruby, C#

## Key Features

### Neural Networks
- **Spiking Neural Networks**: Leaky Integrate-and-Fire (LIF) and Izhikevich neuron models
- **Advanced Plasticity**: STDP, Oja's rule, homeostatic plasticity, eligibility traces
- **Synapse Diversity**: AMPA, NMDA, GABA-A, GABA-B, dopamine, serotonin, acetylcholine
- **Topology Generation**: Scale-free and fractal networks (70-80% fewer connections than random)
- **GPU Acceleration**: CUDA support for high-performance computation

### Cognitive Capabilities
- **Working Memory**: Miller's 7±2 capacity with rehearsal and decay
- **Ethical Reasoning**: Golden Rule implementation with harm prevention
- **Theory of Mind**: Social cognition and empathy modeling
- **Emotional Intelligence**: Emotion recognition, tagging, and regulation
- **Mirror Neurons**: Observational learning from demonstrations
- **Epistemic Filtering**: Bias detection and credibility assessment
- **Mental Health Monitoring**: Well-being tracking and disorder detection

### Multi-Modal Processing
- **Visual Cortex**: CNN-like feature extraction (edges, textures, objects)
- **Audio Cortex**: FFT spectral analysis and frequency processing
- **Language Processing**: NLP integration with word embeddings and reasoning
- **Attention Mechanisms**: Cross-modal attention weighting and integration

### Language Bindings
- Python (primary)
- Go
- Rust
- Java
- Node.js
- Ruby
- C#

### Performance
| Brain Size | Neurons | Memory  | Inference Time |
|------------|---------|---------|----------------|
| Tiny       | 100     | <1 MB   | ~0.1 ms        |
| Small      | 1,000   | ~10 MB  | ~0.5 ms        |
| Medium     | 10,000  | ~50 MB  | ~5 ms          |
| Large      | 100,000 | ~500 MB | ~50 ms         |

## Quick Start

### Installation

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install build-essential cmake python3-dev libjansson-dev liblz4-dev

# Optional: GPU support
sudo apt-get install nvidia-cuda-toolkit

# Clone and build
git clone https://github.com/redmage123/nimcp.git
cd nimcp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run tests
ctest -j$(nproc)

# Install system-wide
sudo make install
```

### Your First NIMCP Program (C)

```c
#include <nimcp.h>

int main(void) {
    // Create a small brain for classification
    nimcp_brain_t brain = nimcp_brain_create(
        "my_first_brain",
        NIMCP_BRAIN_SMALL,           // 1000 neurons
        NIMCP_BRAIN_TASK_CLASSIFICATION,
        8,                           // 8 inputs
        3                            // 3 output classes
    );

    // Train on a simple pattern
    float inputs[8] = {1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0};
    float target[3] = {1.0, 0.0, 0.0};  // Class 0

    float loss = nimcp_brain_learn_example(brain, inputs, 8, target, 3);
    printf("Training loss: %.4f\n", loss);

    // Make predictions
    float outputs[3];
    nimcp_status_t status = nimcp_brain_predict(brain, inputs, 8, outputs, 3);

    printf("Prediction: [%.2f, %.2f, %.2f]\n", outputs[0], outputs[1], outputs[2]);

    // Clean up
    nimcp_brain_destroy(brain);
    return 0;
}
```

### Your First NIMCP Program (Python)

```python
import nimcp
import numpy as np

# Create a small brain
brain = nimcp.Brain(
    name="my_first_brain",
    size=nimcp.BrainSize.SMALL,
    task=nimcp.BrainTask.CLASSIFICATION,
    num_inputs=8,
    num_outputs=3
)

# Train on a simple pattern
inputs = np.array([1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0])
target = np.array([1.0, 0.0, 0.0])  # Class 0

loss = brain.learn_example(inputs, target)
print(f"Training loss: {loss:.4f}")

# Make predictions
prediction = brain.predict(inputs)
print(f"Prediction: {prediction}")
```

## Documentation

### Navigation

| Need | Go To |
|------|-------|
| **Master Index** | [INDEX.md](INDEX.md) |
| **Developer Reference** | [claude/](claude/) |
| **Tutorials** | [tutorials/](tutorials/) |
| **FAQ** | [tutorials/FAQ.md](tutorials/FAQ.md) |

### Getting Started
- **[Getting Started Tutorial](tutorials/GETTING_STARTED.md)** - Step-by-step tutorial
- **[Create Your First Module](tutorials/CREATE_MODULE.md)** - Build your own module
- **[FAQ](tutorials/FAQ.md)** - Common questions and answers
- **[Quick Reference](QUICK_REFERENCE.md)** - Fast command lookup
- **[Simple Start](SIMPLE_START.md)** - Minimal examples

### Core Documentation
- **[Architecture Summary](ARCHITECTURE_SUMMARY.md)** - System architecture and design patterns
- **[API Reference](api/API_REFERENCE.md)** - Complete API documentation
- **[Build System](BUILD_QUICK_REFERENCE.md)** - Build commands and options
- **[Cognitive Modules](COGNITIVE_QUICK_REFERENCE.md)** - Guide to cognitive features

### Advanced Topics
- **[Distributed Copy-on-Write](DISTRIBUTED_COW_README.md)** - Efficient distributed deployment
- **[Multi-Modal Integration](PHASE_8_7_SYNAPSE_TYPES_REPORT.md)** - Vision, audio, language processing
- **[Ethical Guidelines](ETHICAL_GUIDELINES.md)** - Responsible AI development
- **[Security](security/SECURITY.md)** - Security features and best practices

### Examples
Comprehensive examples in `/home/bbrelin/nimcp/examples/`:
- `simple_demo.c` / `simple_demo.py` - Basic usage
- `multimodal_integration_demo.c` - Vision + audio + language
- `ethics_demo.c` - Ethical reasoning demonstration
- `distributed_cow_demo.c` - Distributed deployment
- `working_memory_public_api_demo.c` - Working memory usage
- And 25+ more examples

## Project Structure

```
nimcp/
├── src/
│   ├── include/nimcp.h          # Public API (single header)
│   ├── core/                    # Core neural network engine
│   │   ├── brain/               # High-level brain API
│   │   ├── neuralnet/           # Low-level neural network
│   │   ├── synapse_compute/     # Programmable synapses
│   │   ├── neuron_types/        # Specialized neuron types
│   │   └── topology/            # Network topology generation
│   ├── cognitive/               # Cognitive modules (30+)
│   │   ├── ethics/              # Ethical reasoning
│   │   ├── working_memory/      # Working memory system
│   │   ├── theory_of_mind/      # Social cognition
│   │   └── ...                  # Many more modules
│   ├── plasticity/              # Learning mechanisms
│   ├── glial/                   # Glial cell simulation
│   ├── networking/              # Distributed cognition
│   └── bindings/                # Language bindings
├── test/                        # 383 unit & integration tests
├── examples/                    # Example programs
├── docs/                        # Documentation
└── CMakeLists.txt               # Build configuration
```

## Advanced Features

### Copy-on-Write (COW) Cloning
Create lightweight brain clones with 86% memory savings:

```c
// Create efficient clone
nimcp_brain_t clone = nimcp_brain_clone_cow(original);

// Clone time: <10ms vs ~1000ms full copy
// Memory: ~1MB metadata vs ~50MB full copy
```

### Multi-Modal Processing
Process vision, audio, and language in unified pipeline:

```c
brain_multimodal_input_t input = {
    .visual_data = image_pixels,      // 8x8 grayscale image
    .visual_width = 8,
    .visual_height = 8,
    .audio_data = audio_samples,       // PCM audio
    .audio_samples = 1024,
    .language_text = "What do you see?"
};

brain_multimodal_output_t output;
brain_process_multimodal(brain, &input, &output);

printf("Decision: %s\n", output.decision_label);
printf("Confidence: %.2f\n", output.confidence);
printf("Ethical: %s\n", output.ethical_approved ? "Yes" : "No");
```

### Epistemic Filtering
Detect bias and assess information quality:

```c
// Automatically filters biased or low-quality information
printf("Epistemic Quality: %.2f\n", output.epistemic_quality);
printf("Bias Detected: %s\n", output.bias_detected ? "Yes" : "No");
printf("Credibility: %.2f\n", output.credibility_score);
```

## Building from Source

### Requirements
- **CMake** 3.10+
- **C11** compiler (GCC 7+ or Clang 6+)
- **C++17** compiler (for C++ bindings)
- **Python** 3.7+ with development headers
- **Optional**: CUDA Toolkit 11.0+ for GPU support
- **Optional**: libsodium for encryption

### Build Options

```bash
# Debug build with sanitizers
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON

# Release build with optimizations
cmake .. -DCMAKE_BUILD_TYPE=Release

# With code coverage
cmake .. -DENABLE_COVERAGE=ON
make && ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html

# With fuzzing support
cmake .. -DENABLE_FUZZING=ON
```

## Testing

```bash
# Run all tests
ctest -j$(nproc)

# Run specific test categories
ctest -L unit          # Unit tests only
ctest -L integration   # Integration tests
ctest -L e2e           # End-to-end tests

# Verbose output
ctest -V

# Run with verbose output for details
```

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for:
- Code of conduct
- Development setup
- Coding standards
- Pull request process
- Testing requirements

## Performance Benchmarks

- **Training Speed**: ~1000 examples/sec (Medium brain, CPU)
- **Inference Speed**: ~200 inferences/sec (Medium brain, CPU)
- **GPU Speedup**: 5-10x faster with CUDA
- **Memory Efficiency**: 86% savings with COW cloning
- **Network Sparsity**: 70-80% fewer connections than random networks

## Roadmap

- **v2.7**: Enhanced synapse computation strategies
- **v2.8**: Advanced neuron type specialization
- **v3.0**: Full neuromorphic hardware support
- **v3.2**: Advanced emotional intelligence modules

## Citation

If you use NIMCP in your research, please cite:

```bibtex
@software{nimcp2024,
  title = {NIMCP: Neuromorphic Infant Machine Cognitive Platform},
  author = {Brelin, Braun},
  year = {2024},
  version = {0.9.0-beta},
  url = {https://github.com/redmage123/nimcp}
}
```

## License

License TBD. Please contact braun.brelin@ai-elevate.ai regarding usage.

## Support

- **Documentation**: [docs/](docs/)
- **Issues**: [GitHub Issues](https://github.com/redmage123/nimcp/issues)
- **Email**: braun.brelin@ai-elevate.ai

## Acknowledgments

NIMCP is inspired by decades of neuroscience research and builds upon:
- Izhikevich neuron models (Izhikevich, 2003)
- STDP learning rules (Bi & Poo, 1998)
- Synapse diversity (Destexhe et al., 1994)
- Brain oscillations (Buzsáki & Draguhn, 2004)
- Working memory models (Baddeley & Hitch, 1974)

---

**Built with biological realism. Research in progress.**
