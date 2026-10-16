# NIMCP Progressive Training Framework

## Overview

The Progressive Training Framework implements **curriculum learning** for NIMCP brains, inspired by human developmental stages. Instead of uniform training on mixed data, it progresses through stages of increasing complexity:

```
Infant → Child → Adolescent → Adult
(0-2y)   (2-7y)  (7-18y)      (18+y)
```

## Quick Start

```bash
# 1. Navigate to scripts directory
cd /home/bbrelin/nimcp/scripts

# 2. Run with default configuration
./progressive_training.py --name my_learner

# 3. View results
ls checkpoints/my_learner_*
ls metrics/my_learner_*
```

**Output**:
- Checkpoints after each stage
- Detailed metrics (JSONL format)
- Training summary (JSON)

## Files in This Directory

### Core Framework

| File | Description |
|------|-------------|
| `progressive_training.py` | Main training framework implementation |
| `training_config.json` | Default configuration with all parameters |
| `example_progressive_training.py` | Six usage examples |

### Documentation

| File | Description |
|------|-------------|
| `README_PROGRESSIVE_TRAINING.md` | This file - quick reference |
| `../docs/PROGRESSIVE_TRAINING_GUIDE.md` | Comprehensive guide |

## Training Stages

The framework implements 4 developmental stages:

### Stage 1: Infant (0-2 years)
- **Focus**: Basic sensory patterns
- **Samples**: 5,000
- **Learning Rate**: 0.01
- **Domains**: Sensory, patterns
- **Threshold**: 70% accuracy

### Stage 2: Child (2-7 years)
- **Focus**: Language and simple concepts
- **Samples**: 10,000
- **Learning Rate**: 0.008
- **Domains**: Language, literature, social
- **Threshold**: 75% accuracy

### Stage 3: Adolescent (7-18 years)
- **Focus**: Complex reasoning
- **Samples**: 15,000
- **Learning Rate**: 0.005
- **Domains**: Science, math, history, ethics, art
- **Threshold**: 80% accuracy

### Stage 4: Adult (18+ years)
- **Focus**: Specialized knowledge
- **Samples**: 20,000
- **Learning Rate**: 0.002
- **Domains**: Technical, philosophy, general
- **Threshold**: 85% accuracy

## Key Features

1. **Adaptive Learning Rate**: Decreases within each stage as mastery increases
2. **Multi-Domain Rotation**: Prevents catastrophic forgetting by interleaving domains
3. **Consolidation Periods**: Sleep-like memory strengthening every N samples
4. **Performance Tracking**: Detailed metrics per stage, epoch, domain
5. **Checkpointing**: Automatic saves after each stage

## Usage Examples

### Example 1: Default Training

```bash
./progressive_training.py --name experiment1
```

Uses default 4-stage configuration.

### Example 2: Custom Configuration

```bash
# Save default config as template
./progressive_training.py --save-config --name template

# Edit template_config.json

# Train with custom config
./progressive_training.py --name custom --config template_config.json
```

### Example 3: Run Examples

```bash
# Run all six examples
./example_progressive_training.py --example 0

# Run specific example (1-6)
./example_progressive_training.py --example 3
```

### Example 4: Analyze Results

```python
import json

# Load summary
with open('metrics/my_learner_summary.json', 'r') as f:
    summary = json.load(f)

for stage in summary['stages']:
    print(f"{stage['name']}: {stage['accuracy']:.2%}")
```

## Configuration

### Key Parameters

Edit `training_config.json`:

```json
{
  "stages": [
    {
      "name": "infant",
      "duration_samples": 5000,     // Training samples
      "learning_rate": 0.01,         // Initial LR
      "consolidation_frequency": 500,// Consolidate every N samples
      "domains": ["sensory"],        // Knowledge domains
      "complexity_level": 0.1,       // Problem difficulty (0-1)
      "success_threshold": 0.7       // Accuracy to advance
    }
  ]
}
```

### Available Domains

- **sensory**: Basic sensory processing
- **patterns**: Pattern recognition
- **language**: Language understanding
- **literature**: Stories and narratives
- **social**: Social interactions
- **science**: Scientific concepts
- **mathematics**: Mathematical reasoning
- **history**: Historical events
- **ethics**: Moral reasoning
- **art**: Aesthetic understanding
- **technical**: Technical procedures
- **philosophy**: Abstract concepts
- **general**: General knowledge

## Output Files

### Checkpoints

`checkpoints/{name}_stage_{stage}.json`:

```json
{
  "stage": "infant",
  "samples_trained": 5000,
  "current_accuracy": 0.712,
  "best_accuracy": 0.712,
  "completed": true
}
```

### Metrics

`metrics/{name}_metrics.jsonl` (one line per epoch):

```json
{"stage": "infant", "epoch": 0, "loss": 0.823, "accuracy": 0.623, "learning_rate": 0.01}
{"stage": "infant", "epoch": 1, "loss": 0.701, "accuracy": 0.685, "learning_rate": 0.00875}
```

### Summary

`metrics/{name}_summary.json`:

```json
{
  "total_duration": 312.5,
  "total_samples": 50000,
  "stages": [
    {"name": "infant", "accuracy": 0.712, "samples": 5000},
    {"name": "child", "accuracy": 0.768, "samples": 10000},
    {"name": "adolescent", "accuracy": 0.823, "samples": 15000},
    {"name": "adult", "accuracy": 0.871, "samples": 20000}
  ]
}
```

## Architecture

### Class Hierarchy

```
ProgressiveTrainer
├── StageConfig (dataclass)
├── TrainingMetrics (dataclass)
├── StageProgress (dataclass)
└── Methods:
    ├── train_stage()          # Train single stage
    ├── train_all_stages()     # Train all stages
    ├── _generate_training_data()
    ├── _consolidate_memory()
    ├── _calculate_adaptive_lr()
    ├── _evaluate_stage()
    ├── _save_checkpoint()
    └── _save_metrics()
```

### Training Flow

```
1. Initialize brain (NIMCP)
2. For each stage:
   a. Load stage configuration
   b. Generate training data
   c. For each epoch:
      - Train on batch
      - Adaptive LR decay
      - Periodic consolidation
      - Evaluate on test set
   d. Save checkpoint
3. Final summary
```

## Comparison to TRAINING_REGIMEN

| Aspect | TRAINING_REGIMEN | Progressive Training |
|--------|------------------|---------------------|
| **Purpose** | Create baseline models | Train application models |
| **Duration** | 48 hours (CPU) | 5-30 minutes |
| **Stages** | 10 (sensory → meta-learning) | 4 (infant → adult) |
| **Modules** | All (visual, audio, ethics, etc.) | Configurable domains |
| **Output** | Pre-trained weights | Custom model + metrics |
| **Users** | NIMCP developers | App developers |

**Use TRAINING_REGIMEN for**: Creating distributable baseline models

**Use Progressive Training for**: Application-specific training with custom data

## Integration with NIMCP

### With Python Bindings

```python
import nimcp
from progressive_training import ProgressiveTrainer

trainer = ProgressiveTrainer(name="my_app")
trainer.train_all_stages()

# Brain is now trained and ready
brain = trainer.brain
result = brain.decide(features)
```

### Standalone (Simulation Mode)

If NIMCP bindings not available, runs in simulation mode for testing configuration.

## Best Practices

### 1. Start with Defaults

```bash
# First run - use defaults
./progressive_training.py --name baseline
```

### 2. Adjust Based on Results

```bash
# Save config
./progressive_training.py --save-config --name baseline

# Edit baseline_config.json based on results

# Train with adjustments
./progressive_training.py --name improved --config baseline_config.json
```

### 3. Monitor Consolidation

Rule of thumb: Consolidate every 5-10% of stage duration

```json
// For 5000 samples:
"consolidation_frequency": 500  // 10% (good)
"consolidation_frequency": 50   // 1% (too frequent)
"consolidation_frequency": 2500 // 50% (too rare)
```

### 4. Set Realistic Thresholds

```json
// Formula: threshold ≥ 1/num_classes + 0.2
// For 10 classes:
"success_threshold": 0.7  // 1/10 + 0.6 = 0.7 ✓
"success_threshold": 0.5  // Too low (advances too quickly)
"success_threshold": 0.95 // Too high (may never advance)
```

## Troubleshooting

### Accuracy Not Improving

**Check**:
- Learning rate (0.001 - 0.01 typical)
- Training duration (sufficient samples?)
- Data quality (normalized features?)

### Catastrophic Forgetting

**Solutions**:
- Increase consolidation frequency
- Include earlier domains in later stages
- Stronger consolidation (higher strength)

### Training Too Slow

**Optimizations**:
- Reduce consolidation cycles (10 → 5)
- Increase batch size (32 → 64)
- Smaller test set (1000 → 500)

## Advanced Usage

### Custom Data Loader

Replace `_generate_training_data()`:

```python
class CustomTrainer(ProgressiveTrainer):
    def _generate_training_data(self, stage, num_samples):
        # Load from your dataset
        return load_custom_data(stage.domains, num_samples)
```

### Custom Consolidation

Override `_consolidate_memory()`:

```python
def _consolidate_memory(self, stage):
    if self.brain:
        # Custom consolidation logic
        self.brain.apply_custom_consolidation(
            strategy=stage.name,
            strength=0.2
        )
```

## Performance

Typical training times (simulation mode on modern CPU):

| Stage | Samples | Duration |
|-------|---------|----------|
| Infant | 5,000 | ~45s |
| Child | 10,000 | ~80s |
| Adolescent | 15,000 | ~105s |
| Adult | 20,000 | ~85s |
| **Total** | **50,000** | **~5 min** |

With NIMCP (actual neural network):
- 2-3x slower for small brains (1K neurons)
- 5-10x slower for large brains (100K neurons)
- GPU acceleration available in future versions

## Related Documentation

- **PROGRESSIVE_TRAINING_GUIDE.md**: Comprehensive guide with detailed explanations
- **TRAINING_REGIMEN.md**: Baseline model creation (10-stage pipeline)
- **PRETRAINED_MODELS.md**: Using pre-trained NIMCP models
- **nimcp_knowledge.h**: Multi-domain learning API
- **nimcp_consolidation.h**: Memory consolidation API

## Examples

### Quick Test

```bash
./progressive_training.py --name test
```

### Science-Focused Curriculum

```bash
./example_progressive_training.py --example 2
```

### Analyze Metrics

```bash
./example_progressive_training.py --example 5
```

### Progressive Complexity Demo

```bash
./example_progressive_training.py --example 6
```

## Support

For questions or issues:

1. Check the [Troubleshooting](#troubleshooting) section
2. Review `PROGRESSIVE_TRAINING_GUIDE.md` for detailed guidance
3. Run examples: `./example_progressive_training.py --example 0`
4. File issues on NIMCP repository

## License

Same as NIMCP (MIT License)

---

**Version**: 1.0.0
**Last Updated**: 2025-11-09
**Author**: NIMCP Development Team
