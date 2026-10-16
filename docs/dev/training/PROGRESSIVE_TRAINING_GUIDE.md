# NIMCP Progressive Training Guide

**Version 1.0.0** | **For NIMCP Application Developers**

## Overview

This guide describes the **Progressive Training Framework** - a curriculum learning approach for training NIMCP brains through developmental stages inspired by human cognitive development.

> **Goal**: Train NIMCP brains incrementally from simple to complex concepts
>
> **Audience**: Developers building applications with NIMCP
>
> **Related**: See [TRAINING_REGIMEN.md](TRAINING_REGIMEN.md) for the 10-stage baseline model creation pipeline

## Table of Contents

1. [Philosophy](#philosophy)
2. [Training Stages](#training-stages)
3. [Key Features](#key-features)
4. [Quick Start](#quick-start)
5. [Configuration](#configuration)
6. [Usage Examples](#usage-examples)
7. [Performance Tracking](#performance-tracking)
8. [Best Practices](#best-practices)
9. [Troubleshooting](#troubleshooting)

---

## Philosophy

### Why Progressive Training?

Traditional neural network training uses massive datasets and uniform learning rates. NIMCP's progressive training framework instead mimics **human developmental learning**:

1. **Start Simple**: Begin with basic sensory patterns before complex abstractions
2. **Build Incrementally**: Each stage builds on previous knowledge
3. **Adapt Learning Rate**: Fast initial learning, slower refinement
4. **Prevent Forgetting**: Multi-domain rotation + periodic consolidation
5. **Track Progress**: Detailed metrics at each developmental stage

### Comparison to Standard Training

| Aspect | Standard Training | Progressive Training |
|--------|------------------|---------------------|
| **Curriculum** | Random shuffling | Staged complexity |
| **Learning Rate** | Fixed or scheduled | Adaptive per stage |
| **Domains** | Mixed together | Sequential introduction |
| **Consolidation** | None | Periodic sleep-like strengthening |
| **Metrics** | Overall accuracy | Per-stage, per-domain tracking |

---

## Training Stages

The framework implements **4 developmental stages** inspired by human cognitive development:

### Stage 1: Infant (0-2 years)

**Focus**: Basic sensory patterns and simple associations

- **Duration**: 5,000 samples
- **Learning Rate**: 0.01 (highest)
- **Domains**: Sensory processing, pattern recognition
- **Complexity**: 0.1 (simplest)
- **Success Threshold**: 70% accuracy

**What it learns**:
- Basic feature detection
- Simple pattern matching
- Elementary associations

**Analogous to**: V1 visual cortex training (edges, orientations) from TRAINING_REGIMEN Stage 1-2

### Stage 2: Child (2-7 years)

**Focus**: Language acquisition and simple concepts

- **Duration**: 10,000 samples
- **Learning Rate**: 0.008
- **Domains**: Language, literature, social interaction
- **Complexity**: 0.3
- **Success Threshold**: 75% accuracy

**What it learns**:
- Word meanings and grammar
- Simple narratives and stories
- Basic social concepts

**Analogous to**: Speech processing and multimodal integration from TRAINING_REGIMEN Stage 3-4

### Stage 3: Adolescent (7-18 years)

**Focus**: Abstract reasoning across multiple domains

- **Duration**: 15,000 samples
- **Learning Rate**: 0.005
- **Domains**: Science, mathematics, history, ethics, art
- **Complexity**: 0.6
- **Success Threshold**: 80% accuracy

**What it learns**:
- Scientific principles
- Mathematical reasoning
- Historical causality
- Ethical frameworks
- Aesthetic appreciation

**Analogous to**: Cognitive development and ethical reasoning from TRAINING_REGIMEN Stage 5-7

### Stage 4: Adult (18+ years)

**Focus**: Specialized knowledge and refinement

- **Duration**: 20,000 samples
- **Learning Rate**: 0.002 (lowest)
- **Domains**: Technical skills, philosophy, general knowledge
- **Complexity**: 0.9 (highest)
- **Success Threshold**: 85% accuracy

**What it learns**:
- Complex technical procedures
- Abstract philosophical concepts
- Specialized domain expertise
- Integration across all previous learning

**Analogous to**: Meta-learning and lifelong learning from TRAINING_REGIMEN Stage 9-10

---

## Key Features

### 1. Adaptive Learning Rate

Learning rate decreases **within each stage** as the brain masters concepts:

```python
# Starts at stage.learning_rate, decays to 50% by stage end
current_lr = base_lr * (1 - 0.5 * progress)

# Example for Infant stage:
# Start: 0.01
# Middle: 0.0075
# End: 0.005
```

**Why**: Mimics biological learning - fast acquisition, slow refinement

### 2. Multi-Domain Rotation

Training rotates through domains within each stage:

```
Stage 2 (Child):
  Sample 1: Language
  Sample 2: Literature
  Sample 3: Social
  Sample 4: Language (back to start)
  ...
```

**Why**: Prevents catastrophic forgetting by interleaving domains

### 3. Consolidation Periods

Periodic "sleep-like" consolidation strengthens important patterns:

```python
# Consolidate every N samples
if samples_trained % consolidation_frequency == 0:
    consolidate_memory(brain)
```

**Consolidation performs**:
- Pattern replay (strengthen important associations)
- Synaptic scaling (normalize connection strengths)
- Pruning weak connections (forget unimportant patterns)

**Why**: Based on biological sleep consolidation - critical for long-term retention

### 4. Performance Tracking

Comprehensive metrics collected at each stage:

- Loss and accuracy per epoch
- Per-domain performance
- Learning rate evolution
- Samples trained
- Time elapsed

**Output**: JSONL metrics file + final summary JSON

### 5. Checkpointing

Automatic checkpoints after each stage:

```
checkpoints/
├── learner_stage_infant.json
├── learner_stage_child.json
├── learner_stage_adolescent.json
└── learner_stage_adult.json
```

**Why**: Enables rollback, stage-by-stage analysis, resuming training

---

## Quick Start

### Installation

```bash
# 1. Build NIMCP with Python bindings
cd /home/bbrelin/nimcp/build
cmake ..
make

# 2. Make progressive_training.py executable
chmod +x /home/bbrelin/nimcp/scripts/progressive_training.py
```

### Basic Usage

```bash
# Train with default configuration
cd /home/bbrelin/nimcp/scripts
./progressive_training.py --name my_first_learner

# Output:
# checkpoints/my_first_learner_stage_*.json
# metrics/my_first_learner_metrics.jsonl
# metrics/my_first_learner_summary.json
```

### With Custom Configuration

```bash
# 1. Save default config as template
./progressive_training.py --save-config --name template

# 2. Edit template_config.json (modify stages, learning rates, etc.)

# 3. Train with custom config
./progressive_training.py --name custom_run --config template_config.json
```

---

## Configuration

### Configuration File Structure

```json
{
  "name": "my_training_run",
  "stages": [
    {
      "name": "infant",
      "description": "Basic sensory patterns",
      "age_range": "0-2 years",
      "duration_samples": 5000,
      "learning_rate": 0.01,
      "consolidation_frequency": 500,
      "domains": ["sensory", "patterns"],
      "complexity_level": 0.1,
      "success_threshold": 0.7
    },
    // ... more stages
  ],
  "hyperparameters": {
    "batch_size": 32,
    "lr_decay_factor": 0.5,
    "consolidation_cycles": 10
  }
}
```

### Key Parameters

#### Per-Stage Configuration

| Parameter | Description | Typical Range |
|-----------|-------------|---------------|
| `duration_samples` | Number of training samples | 1,000 - 50,000 |
| `learning_rate` | Starting learning rate | 0.001 - 0.01 |
| `consolidation_frequency` | Samples between consolidation | 100 - 2,000 |
| `domains` | List of knowledge domains | See domain list |
| `complexity_level` | Problem complexity (0-1) | 0.1 - 0.9 |
| `success_threshold` | Accuracy to advance | 0.6 - 0.9 |

#### Global Hyperparameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `batch_size` | Samples per batch | 32 |
| `samples_per_epoch` | Samples per epoch | 1,000 |
| `test_set_size` | Test set size | 1,000 |
| `lr_decay_factor` | LR decay rate | 0.5 |
| `consolidation_cycles` | Consolidation iterations | 10 |
| `consolidation_strength` | Consolidation learning rate | 0.1 |
| `pruning_threshold` | Weak connection threshold | 0.01 |

### Available Domains

The framework supports **13 knowledge domains**:

1. **sensory**: Basic sensory processing
2. **patterns**: Pattern recognition
3. **language**: Language understanding
4. **literature**: Stories and narratives
5. **social**: Social interactions
6. **science**: Scientific concepts
7. **mathematics**: Mathematical reasoning
8. **history**: Historical events and causality
9. **ethics**: Moral reasoning
10. **art**: Aesthetic understanding
11. **technical**: Technical procedures
12. **philosophy**: Abstract philosophy
13. **general**: General world knowledge

Each domain has configurable:
- Feature dimensionality
- Number of classes
- Complexity characteristics

---

## Usage Examples

### Example 1: Quick Test Run

```bash
# Fast test with reduced samples
./progressive_training.py --name quick_test
```

Output:
```
======================================================================
Stage 1/4: INFANT
Age Range: 0-2 years
Description: Basic sensory patterns and simple associations
Domains: sensory, patterns
Complexity: 10.0%
======================================================================

Generating 5000 training samples...
Training on 5000 samples across domains: ['sensory', 'patterns']

--- Epoch 1/5 ---
  Loss: 0.8234 | Accuracy: 62.30% | LR: 0.010000 | Samples: 1000/5000

  Consolidating memory (strengthening important patterns)...

--- Epoch 2/5 ---
  Loss: 0.7012 | Accuracy: 68.50% | LR: 0.008750 | Samples: 2000/5000
...

Stage infant completed!
  Duration: 45.2s (0.8 min)
  Samples trained: 5,000
  Final accuracy: 71.20%
  Best accuracy: 71.20%
```

### Example 2: Custom Learning Rates

```json
// custom_lr_config.json
{
  "stages": [
    {
      "name": "infant",
      "learning_rate": 0.02,  // Higher initial LR
      "duration_samples": 3000,  // Fewer samples
      // ... other params
    },
    {
      "name": "child",
      "learning_rate": 0.015,  // Still high
      "duration_samples": 5000,
      // ...
    }
  ]
}
```

```bash
./progressive_training.py --name fast_learner --config custom_lr_config.json
```

### Example 3: Focus on Specific Domains

```json
// science_focused_config.json
{
  "stages": [
    {
      "name": "scientific_foundation",
      "domains": ["science", "mathematics"],
      "duration_samples": 15000,
      "learning_rate": 0.005
    },
    {
      "name": "scientific_specialization",
      "domains": ["science", "technical"],
      "duration_samples": 20000,
      "learning_rate": 0.002
    }
  ]
}
```

### Example 4: Analyzing Metrics

```python
#!/usr/bin/env python3
"""Analyze progressive training metrics"""

import json

# Load metrics
with open('metrics/my_learner_metrics.jsonl', 'r') as f:
    metrics = [json.loads(line) for line in f]

# Analyze accuracy progression
infant_metrics = [m for m in metrics if m['stage'] == 'infant']
print(f"Infant stage accuracy range: {min(m['accuracy'] for m in infant_metrics):.2%} - {max(m['accuracy'] for m in infant_metrics):.2%}")

# Plot learning curves
import matplotlib.pyplot as plt

stages = ['infant', 'child', 'adolescent', 'adult']
colors = ['blue', 'green', 'orange', 'red']

for stage, color in zip(stages, colors):
    stage_data = [m for m in metrics if m['stage'] == stage]
    accuracies = [m['accuracy'] for m in stage_data]
    plt.plot(accuracies, label=stage, color=color)

plt.xlabel('Epoch')
plt.ylabel('Accuracy')
plt.title('Progressive Training Learning Curves')
plt.legend()
plt.savefig('learning_curves.png')
```

---

## Performance Tracking

### Metrics File Format

**JSONL format** (one JSON object per line):

```json
{"stage": "infant", "epoch": 0, "sample": 1000, "loss": 0.823, "accuracy": 0.623, "domain": "sensory", "timestamp": 1699123456.789, "learning_rate": 0.01}
{"stage": "infant", "epoch": 1, "sample": 2000, "loss": 0.701, "accuracy": 0.685, "domain": "patterns", "timestamp": 1699123467.234, "learning_rate": 0.00875}
...
```

### Summary File Format

**JSON format** (human-readable summary):

```json
{
  "name": "my_learner",
  "total_duration": 312.5,
  "total_samples": 50000,
  "stages": [
    {
      "name": "infant",
      "accuracy": 0.712,
      "samples": 5000,
      "duration": 45.2
    },
    {
      "name": "child",
      "accuracy": 0.768,
      "samples": 10000,
      "duration": 78.6
    },
    {
      "name": "adolescent",
      "accuracy": 0.823,
      "samples": 15000,
      "duration": 104.3
    },
    {
      "name": "adult",
      "accuracy": 0.871,
      "samples": 20000,
      "duration": 84.4
    }
  ],
  "completed_at": "2025-11-09T10:30:45.123456"
}
```

### Checkpoint Format

**JSON format** (stage checkpoint):

```json
{
  "name": "my_learner",
  "stage": "infant",
  "progress": {
    "stage_name": "infant",
    "samples_trained": 5000,
    "total_samples": 5000,
    "current_accuracy": 0.712,
    "best_accuracy": 0.712,
    "epochs_completed": 5,
    "start_time": 1699123456.0,
    "end_time": 1699123501.2,
    "completed": true
  },
  "global_step": 5000,
  "timestamp": "2025-11-09T10:25:01.234567"
}
```

---

## Best Practices

### 1. Start with Default Configuration

**Do**:
```bash
# Use defaults for first run
./progressive_training.py --name first_run
```

**Don't**:
```bash
# Don't immediately customize everything
./progressive_training.py --name first_run --config ultra_custom.json
```

**Why**: Defaults are tuned for balanced learning. Customize only after understanding baseline performance.

### 2. Monitor Consolidation Effectiveness

Check if consolidation frequency is appropriate:

```python
# Too frequent (overhead > benefit):
"consolidation_frequency": 50  # Every 50 samples

# Too rare (forgetting before consolidation):
"consolidation_frequency": 5000  # Every 5000 samples

# Good balance:
"consolidation_frequency": 500  # ~10% of stage duration
```

**Rule of thumb**: Consolidate every 5-10% of stage duration

### 3. Set Realistic Success Thresholds

```python
# Too low (advances too quickly):
"success_threshold": 0.5

# Too high (may never advance):
"success_threshold": 0.99

# Balanced (slightly above random for multi-class):
"success_threshold": 0.7  # For ~10 classes
```

**Formula**: `success_threshold ≥ 1/num_classes + 0.2`

### 4. Adjust Complexity Gradually

```python
# Good: Gradual increase
stages = [
    {"complexity_level": 0.1},  # Infant
    {"complexity_level": 0.3},  # Child (+0.2)
    {"complexity_level": 0.6},  # Adolescent (+0.3)
    {"complexity_level": 0.9}   # Adult (+0.3)
]

# Bad: Sharp jumps
stages = [
    {"complexity_level": 0.1},  # Infant
    {"complexity_level": 0.9},  # Child (+0.8, too steep!)
]
```

**Why**: Sharp complexity increases cause performance collapse

### 5. Use Checkpoints for Experimentation

```bash
# Train baseline
./progressive_training.py --name baseline

# Load infant checkpoint, try different Stage 2 configs
# (In production, would implement checkpoint loading)
./progressive_training.py --name experiment1 --resume checkpoints/baseline_stage_infant.json
```

---

## Troubleshooting

### Problem: Accuracy Not Improving

**Symptoms**: Accuracy stuck near random chance (e.g., 10% for 10 classes)

**Solutions**:

1. **Check Learning Rate**:
   ```json
   // Too low
   "learning_rate": 0.0001  // Increase to 0.001-0.01

   // Too high (loss exploding)
   "learning_rate": 0.1  // Decrease to 0.01
   ```

2. **Increase Training Duration**:
   ```json
   "duration_samples": 5000  // Too few
   "duration_samples": 10000  // Better
   ```

3. **Verify Data Quality**:
   - Check that features are normalized (0-1 range)
   - Ensure labels are consistent
   - Verify domain rotation is working

### Problem: Catastrophic Forgetting

**Symptoms**: Later stages perform worse on early domains

**Solutions**:

1. **Increase Consolidation Frequency**:
   ```json
   "consolidation_frequency": 2000  // Too rare
   "consolidation_frequency": 500   // Better
   ```

2. **Multi-Domain Review**:
   - Include earlier domains in later stages
   - Example: Add "language" to adolescent stage

3. **Stronger Consolidation**:
   ```json
   "consolidation_strength": 0.05  // Too weak
   "consolidation_strength": 0.15  // Stronger
   ```

### Problem: Training Too Slow

**Symptoms**: Hours per stage, low samples/sec

**Solutions**:

1. **Reduce Consolidation Overhead**:
   ```json
   "consolidation_cycles": 20  // Too many
   "consolidation_cycles": 5   // Faster
   ```

2. **Increase Batch Size**:
   ```json
   "batch_size": 8   // Too small
   "batch_size": 64  // Faster
   ```

3. **Use Smaller Test Set**:
   ```json
   "test_set_size": 5000  // Excessive
   "test_set_size": 500   // Adequate
   ```

### Problem: Stage Never Reaches Success Threshold

**Symptoms**: Accuracy plateaus below threshold

**Solutions**:

1. **Lower Threshold**:
   ```json
   "success_threshold": 0.95  // Too high
   "success_threshold": 0.75  // Realistic
   ```

2. **Increase Stage Duration**:
   ```json
   "duration_samples": 3000   // Too short
   "duration_samples": 10000  // More learning time
   ```

3. **Check Data Difficulty**:
   - Reduce `complexity_level` if too hard
   - Verify features are informative for labels

---

## Integration with TRAINING_REGIMEN

The Progressive Training Framework is **complementary** to the baseline TRAINING_REGIMEN:

| TRAINING_REGIMEN | Progressive Training |
|------------------|---------------------|
| **Purpose**: Create pre-trained baseline models | **Purpose**: Train application-specific models |
| **Stages**: 10 stages (sensory → meta-learning) | **Stages**: 4 stages (infant → adult) |
| **Duration**: ~48 hours (CPU) / ~8 hours (GPU) | **Duration**: ~5-30 minutes |
| **Output**: Baseline weights for distribution | **Output**: Application model + metrics |
| **Scope**: All NIMCP modules (visual, audio, speech, ethics, logic) | **Scope**: Configurable domains |
| **Users**: NIMCP core developers | **Users**: Application developers |

### When to Use Which

**Use TRAINING_REGIMEN when**:
- Creating pre-trained models for distribution
- Need comprehensive multi-modal capabilities
- Building NIMCP baselines for research

**Use Progressive Training when**:
- Training domain-specific applications
- Need fast iteration on custom datasets
- Want detailed per-stage metrics
- Experimenting with curriculum learning

**Use Both**:
- Start with TRAINING_REGIMEN baseline
- Fine-tune with Progressive Training on your data

---

## Future Enhancements

Planned improvements to the framework:

1. **Real Data Loaders**: Integration with actual datasets (CIFAR, ImageNet, etc.)
2. **Transfer Learning**: Load TRAINING_REGIMEN baselines as starting point
3. **Multi-GPU Support**: Parallel training across GPUs
4. **Hyperparameter Optimization**: Automatic tuning of learning rates, thresholds
5. **Visualization Dashboard**: Real-time training visualization
6. **Resume Capability**: Resume from checkpoint mid-stage
7. **A/B Testing**: Compare multiple configurations in parallel

---

## References

- **TRAINING_REGIMEN.md**: Baseline model creation pipeline
- **PRETRAINED_MODELS.md**: Using pre-trained NIMCP models
- **nimcp_knowledge.h**: Multi-domain learning API
- **nimcp_consolidation.h**: Memory consolidation API
- **python_brain_cow_demo.py**: Python API examples

---

## Support

Questions or issues with progressive training?

1. Check [Troubleshooting](#troubleshooting) section above
2. Review example configurations in `/home/bbrelin/nimcp/scripts/`
3. See TRAINING_REGIMEN.md for related guidance
4. File issues on NIMCP repository

---

**Documentation Version**: 1.0.0
**Last Updated**: 2025-11-09
**Author**: NIMCP Development Team
