# NIMCP Training Pipeline

Complete training pipeline for NIMCP foundation models with dataset downloading, preprocessing, and progressive multi-domain training.

## Overview

The training pipeline consists of three main components:

1. **Dataset Downloader** (`download_datasets.py`) - Downloads and extracts datasets
2. **Data Preprocessor** (`preprocess_datasets.py`) - Converts datasets to NIMCP format
3. **Training Script** (`train_foundation_model.py`) - Trains foundation models

## Directory Structure

```
nimcp/
├── datasets/
│   ├── raw/           # Downloaded archives
│   ├── extracted/     # Extracted datasets
│   └── processed/     # Preprocessed NIMCP format
│       └── {dataset}/
│           ├── train.jsonl
│           ├── val.jsonl
│           ├── test.jsonl
│           ├── metadata.json
│           └── vocab.json
├── models/            # Trained models and checkpoints
│   └── {run_name}/
│       ├── checkpoint_*.bin
│       ├── foundation_model_final.bin
│       ├── training_metrics.json
│       └── foundation_model_stats.json
└── scripts/           # Training pipeline scripts
    ├── download_datasets.py
    ├── preprocess_datasets.py
    ├── train_foundation_model.py
    ├── run_full_pipeline.py
    └── example_*.json
```

## Quick Start

### 1. Complete Pipeline (Recommended)

Run the entire pipeline with one command:

```bash
cd scripts
python run_full_pipeline.py --config example_datasets_config.json
```

This will:
- Download all datasets
- Preprocess them
- Train a foundation model
- Save checkpoints and metrics

### 2. Step-by-Step

#### Step 1: Download Datasets

```bash
python download_datasets.py --config example_datasets_config.json
```

Or download a single dataset:

```bash
python download_datasets.py \
  --url https://example.com/data.zip \
  --name my_dataset \
  --md5 abc123...
```

#### Step 2: Preprocess Datasets

```bash
python preprocess_datasets.py --config example_preprocess_config.json
```

Or preprocess a single dataset:

```bash
python preprocess_datasets.py \
  --dataset my_dataset \
  --source my_dataset \
  --format csv \
  --max-features 512
```

#### Step 3: Train Foundation Model

```bash
python train_foundation_model.py --config example_training_config.json
```

## Configuration Files

### Dataset Download Config (`example_datasets_config.json`)

```json
{
  "datasets": [
    {
      "name": "dataset_name",
      "url": "https://example.com/data.tar.gz",
      "md5": "optional_checksum",
      "extract_to": "optional_subdir"
    }
  ]
}
```

### Preprocessing Config (`example_preprocess_config.json`)

```json
{
  "datasets": [
    {
      "name": "dataset_name",
      "domain": "classification",
      "source_dir": "dataset_name",
      "format": "csv",
      "text_column": "text",
      "label_column": "label",
      "max_features": 512,
      "train_ratio": 0.7,
      "val_ratio": 0.15,
      "test_ratio": 0.15
    }
  ]
}
```

Supported formats:
- `csv` - CSV files with text and label columns
- `jsonl` - JSON Lines format
- `folders` - Each subfolder is a class, containing text files

### Training Config (`example_training_config.json`)

```json
{
  "brain": {
    "name": "foundation_model_v1",
    "size": 2,
    "task": 0,
    "num_inputs": 512,
    "num_outputs": 100
  },
  "domains": [
    {"name": "dataset1"},
    {"name": "dataset2"}
  ],
  "training": {
    "learning_rate": 0.01,
    "lr_decay": 0.95,
    "min_lr": 0.0001,
    "batch_size": 32,
    "epochs_per_domain": 5,
    "consolidation_epochs": 2,
    "early_stop_patience": 3,
    "checkpoint_interval": 1
  },
  "datasets_dir": "../datasets/processed",
  "output_dir": "../models/foundation_v1"
}
```

Brain sizes:
- 0 = TINY (100 neurons)
- 1 = SMALL (1,000 neurons)
- 2 = MEDIUM (10,000 neurons)
- 3 = LARGE (100,000 neurons)

Task types:
- 0 = CLASSIFICATION
- 1 = REGRESSION
- 2 = PATTERN_MATCHING
- 3 = SEQUENCE
- 4 = ASSOCIATION

## Training Features

### Progressive Multi-Domain Training

The trainer trains on multiple domains sequentially:

1. **Domain Training** - Train on domain A
2. **Consolidation** - Replay samples from previous domains
3. **Domain Training** - Train on domain B
4. **Consolidation** - Replay samples from A and B
5. Repeat...

This prevents catastrophic forgetting and builds a robust foundation model.

### Curriculum Learning

Organize domains from easy to hard in your config:

```json
"domains": [
  {"name": "binary_classification"},  // Easy
  {"name": "sentiment_analysis"},     // Medium
  {"name": "multi_class_news"},       // Hard
  {"name": "topic_classification"}    // Hardest
]
```

### Adaptive Learning Rate

Learning rate automatically decays during training:
- Initial LR: 0.01
- Decay: 0.95 per epoch
- Minimum LR: 0.0001

### Early Stopping

Training stops early if validation accuracy doesn't improve for N epochs (default: 3).

### Checkpointing

Models are saved:
- After each domain
- At regular intervals during training (configurable)
- Final model at end

## Monitoring Training

### Real-Time Progress

Training prints progress in real-time:

```
==============================================================
Training Domain: imdb_sentiment
==============================================================
Loading data...
  Train samples: 25000
  Val samples: 5357

Epoch 1/5
  Train Acc: 0.7234
  Val Acc: 0.7156
  Val Conf: 0.8234
  LR: 0.009500
  Time: 45.23s
  Saving checkpoint: checkpoint_imdb_sentiment_epoch0_20250109_143022.bin
```

### Metrics JSON

All metrics are saved to `training_metrics.json`:

```json
{
  "train_accuracy": [
    {"value": 0.7234, "epoch": 0, "timestamp": 1234567890}
  ],
  "val_accuracy": [...],
  "val_confidence": [...],
  "learning_rate": [...],
  "consolidation_accuracy": [...]
}
```

### Brain Statistics

Final brain statistics in `foundation_model_stats.json`:

```json
{
  "task_name": "CLASSIFICATION",
  "num_neurons": 10000,
  "num_synapses": 150000,
  "total_learning_steps": 125000,
  "memory_bytes": 52428800,
  "accuracy": 0.8456
}
```

## Advanced Usage

### Resume Training

Resume from a checkpoint:

```bash
python train_foundation_model.py \
  --config training_config.json \
  --resume ../models/checkpoint_epoch5.bin
```

### Custom Paths

Use custom directories:

```bash
python train_foundation_model.py \
  --config training_config.json \
  --datasets-dir /path/to/datasets \
  --output /path/to/models
```

### Dry Run

Check configuration without training:

```bash
python train_foundation_model.py \
  --config training_config.json \
  --dry-run
```

## Memory Management

### Memory Requirements

Approximate memory usage per brain size:

- TINY: ~10 MB
- SMALL: ~50 MB
- MEDIUM: ~200 MB
- LARGE: ~1 GB

### Batch Size

Adjust batch size based on available memory:

```json
"training": {
  "batch_size": 32  // Decrease if out of memory
}
```

### COW Cloning

For parallel inference, use COW (Copy-on-Write) cloning:

```python
import nimcp

# Load trained model
brain = nimcp.Brain.load("foundation_model_final.bin")

# Create efficient clones for parallel inference
clones = [brain.clone_cow() for _ in range(10)]

# Use clones independently (shares 86% of memory)
for clone in clones:
    label, conf = clone.decide(features)
```

## Performance Tips

1. **Start Small** - Use TINY or SMALL brains for testing
2. **Increase Batch Size** - Larger batches = faster training (if memory allows)
3. **Reduce Features** - Start with 256 features instead of 512
4. **Use Fewer Epochs** - Start with 2-3 epochs per domain
5. **Sample Datasets** - Test on subset before full training

## Troubleshooting

### Out of Memory

- Reduce brain size
- Reduce batch size
- Reduce max_features
- Use fewer training samples

### Low Accuracy

- Increase epochs_per_domain
- Increase consolidation_epochs
- Adjust learning rate
- Check data quality
- Increase brain size

### Slow Training

- Increase batch size
- Use smaller brain
- Reduce feature dimensions
- Use faster hardware

### Dataset Issues

Check preprocessing output:

```bash
cat ../datasets/processed/{dataset}/metadata.json
```

Verify data format:

```bash
head -n 5 ../datasets/processed/{dataset}/train.jsonl
```

## Example Datasets

The example config includes these public datasets:

1. **IMDB Sentiment** - Movie reviews (positive/negative)
2. **AG News** - News articles (4 categories)
3. **SMS Spam** - Spam classification (spam/ham)
4. **20 Newsgroups** - Discussion posts (20 topics)

## Integration with NIMCP

### Using Trained Models

```python
import nimcp

# Load trained foundation model
brain = nimcp.Brain.load("../models/foundation_v1/foundation_model_final.bin")

# Make predictions
features = [0.1, 0.2, ...]  # 512-dim feature vector
label, confidence = brain.decide(features)

print(f"Prediction: {label} (confidence: {confidence:.3f})")

# Continue training (fine-tuning)
brain.learn(features, "new_label", confidence=0.9)

# Save fine-tuned model
brain.save("fine_tuned_model.bin")
```

### Model Statistics

```python
stats = brain.probe()

print(f"Neurons: {stats['num_neurons']:,}")
print(f"Synapses: {stats['num_synapses']:,}")
print(f"Learning steps: {stats['total_learning_steps']:,}")
print(f"Memory: {stats['memory_bytes'] / 1024 / 1024:.2f} MB")
```

## Next Steps

1. **Download datasets** - Run download script
2. **Preprocess data** - Convert to NIMCP format
3. **Train model** - Start with example config
4. **Evaluate** - Check validation accuracy
5. **Fine-tune** - Adjust hyperparameters
6. **Deploy** - Use trained model for inference

## Support

For issues or questions:
- Check this README
- Review example configs
- Check NIMCP documentation
- Examine training logs and metrics

## License

NIMCP Training Pipeline
Copyright (c) 2025 NIMCP Development Team
