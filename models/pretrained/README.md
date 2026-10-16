# NIMCP Pre-trained Models Repository

This directory contains pre-trained NIMCP models organized by size and version.

## Directory Structure

```
pretrained/
├── small/          # Small models (1K-5K neurons)
│   ├── v1.0/       # Version 1.0 releases
│   └── v1.1/       # Version 1.1 releases
├── medium/         # Medium models (5K-20K neurons)
│   ├── v1.0/
│   └── v1.1/
└── large/          # Large models (20K+ neurons)
    ├── v1.0/
    └── v1.1/
```

## Model Naming Convention

Models follow the naming pattern:
```
nimcp_<type>_<size>_<version>
```

Examples:
- `nimcp_foundation_small_v1.0` - General-purpose small model
- `nimcp_ethics_medium_v1.0` - Ethics-specialized medium model
- `nimcp_multimodal_large_v1.0` - Large multimodal model

## File Types

Each model version contains:

1. **Model Binary** (`*.nimcp`)
   - Serialized neural network state
   - Synaptic weights and topology
   - Neural parameters

2. **Metadata** (`*.json`)
   - Model specifications
   - Performance metrics
   - Training information

3. **Checksum** (`*.sha256`)
   - File integrity verification
   - Security validation

## Available Models

### Small Models (Fast Inference, Low Memory)

**nimcp_foundation_small_v1.0**
- Neurons: 2,500
- Domains: Ethics, Logic, Basic Reasoning
- Accuracy: 85-88%
- Memory: ~5 MB
- Use Case: Edge devices, quick decisions

### Medium Models (Balanced Performance)

**nimcp_foundation_medium_v1.0**
- Neurons: 10,000
- Domains: Ethics, Philosophy, Physics, Logic, Math
- Accuracy: 89-92%
- Memory: ~42 MB
- Use Case: General-purpose applications

**nimcp_ethics_medium_v1.0**
- Neurons: 8,000
- Domains: Ethics (specialized)
- Accuracy: 94%
- Memory: ~35 MB
- Use Case: Ethical decision-making systems

### Large Models (Maximum Performance)

**nimcp_foundation_large_v1.0**
- Neurons: 50,000
- Domains: All domains with high accuracy
- Accuracy: 93-95%
- Memory: ~250 MB
- Use Case: Server-side, research applications

**nimcp_multimodal_large_v1.0**
- Neurons: 75,000
- Domains: Vision, Audio, Speech, Reasoning
- Accuracy: 91-94%
- Memory: ~380 MB
- Use Case: Multi-modal AI applications

## Usage

### C API

```c
#include "core/brain/nimcp_pretrained.h"

// Load pre-trained model
brain_t brain = brain_load_pretrained(
    "nimcp_foundation_medium_v1.0",
    NULL  // Use default models directory
);

// Use immediately for inference
brain_decision_t* decision = nimcp_brain_decide(brain, features, num_features);

// Optional: Fine-tune on your specific data
nimcp_brain_learn_example(brain, task_features, num_features, label, 1.0f);
```

### Python API

```python
import nimcp

# Load pre-trained model
brain = nimcp.Brain.from_pretrained("foundation_medium_v1.0")

# Use immediately
result = brain.decide(features)

# Optional: Fine-tune
brain.learn(task_data, task_labels)
```

## Model Selection Guide

| Use Case | Recommended Model | Rationale |
|----------|------------------|-----------|
| Edge/IoT | small_v1.0 | Low memory, fast inference |
| Mobile Apps | small_v1.0 or medium_v1.0 | Balance of performance and resources |
| Web Services | medium_v1.0 | Good performance, reasonable memory |
| Ethical Systems | ethics_medium_v1.0 | Specialized for ethics domain |
| Research/Analysis | large_v1.0 | Maximum accuracy |
| Multi-modal AI | multimodal_large_v1.0 | Vision + Audio + Speech |

## Performance Benchmarks

| Model | Neurons | Inference (ms) | Memory (MB) | Avg Accuracy |
|-------|---------|----------------|-------------|--------------|
| small_v1.0 | 2,500 | 2-5 | 5 | 86% |
| medium_v1.0 | 10,000 | 8-15 | 42 | 90% |
| large_v1.0 | 50,000 | 40-80 | 250 | 94% |

*Benchmarks on Intel Core i7-10700K, single-threaded*

## Fine-tuning Guidelines

Pre-trained models can be fine-tuned for specific tasks:

1. **Start with appropriate base model**
   - Use ethics_medium for ethics-related tasks
   - Use foundation_* for general tasks

2. **Fine-tune with domain-specific data**
   - 100-1000 examples recommended
   - Higher confidence for critical examples

3. **Monitor performance**
   - Track accuracy on validation set
   - Prevent overfitting on small datasets

4. **Save fine-tuned version**
   - Use `nimcp_brain_save()` to persist
   - Version your fine-tuned models

## License

All pre-trained models are released under the MIT License.

## Contributing Models

To contribute a pre-trained model:

1. Train your model using NIMCP APIs
2. Validate performance using `scripts/validate_pretrained.py`
3. Generate metadata JSON with all required fields
4. Create SHA256 checksum
5. Submit PR with model, metadata, and validation results

## Version History

### v1.1 (Upcoming)
- Improved training procedures
- Enhanced multi-modal integration
- Better epistemic filtering

### v1.0 (Current)
- Initial pre-trained model release
- Foundation models for all sizes
- Specialized ethics model
- Multi-modal model
