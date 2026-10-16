# NIMCP Training Pipeline - Quick Start Guide

Get up and running with NIMCP foundation model training in 5 minutes.

## Prerequisites

1. **Build NIMCP Python module:**
   ```bash
   cd /home/bbrelin/nimcp/build
   make nimcp_python
   export PYTHONPATH=/home/bbrelin/nimcp/build/src/python:$PYTHONPATH
   ```

2. **Verify installation:**
   ```bash
   python3 -c "import nimcp; print('NIMCP version:', nimcp.__name__)"
   ```

## Method 1: One-Command Pipeline (Recommended)

Run the complete pipeline with example datasets:

```bash
cd /home/bbrelin/nimcp/scripts

# Run full pipeline: download -> preprocess -> train
python run_full_pipeline.py --config example_pipeline_config.json
```

This will:
- Download 4 text classification datasets
- Preprocess them into NIMCP format
- Train a foundation model with progressive learning
- Save checkpoints and metrics

**Time:** 1-2 hours (depending on network and CPU)

## Method 2: Step-by-Step

### Step 1: Download Datasets (5-10 minutes)

```bash
python download_datasets.py --config example_datasets_config.json
```

Output location: `../datasets/raw/` and `../datasets/extracted/`

### Step 2: Preprocess Data (5-10 minutes)

```bash
python preprocess_datasets.py --config example_preprocess_config.json
```

Output location: `../datasets/processed/`

### Step 3: Train Model (30-60 minutes)

```bash
python train_foundation_model.py --config example_training_config.json
```

Output location: `../models/foundation_v1/`

## Test Your Trained Model

After training completes:

```bash
# Test on a dataset
python test_trained_model.py \
  --model ../models/foundation_v1/foundation_model_final.bin \
  --dataset ../datasets/processed/imdb_sentiment \
  --split test
```

## Analyze Training Results

```bash
# View training analysis
python analyze_training.py \
  --metrics ../models/foundation_v1/training_metrics.json

# Export summary
python analyze_training.py \
  --metrics ../models/foundation_v1/training_metrics.json \
  --export
```

## Quick Test (No Download Required)

If you want to test the training pipeline without downloading datasets:

1. Create a small synthetic dataset:

```bash
python << 'EOF'
import json
import random
from pathlib import Path

# Create synthetic dataset
output_dir = Path('../datasets/processed/synthetic_test')
output_dir.mkdir(parents=True, exist_ok=True)

classes = ['positive', 'negative', 'neutral']

for split, count in [('train', 100), ('val', 20), ('test', 20)]:
    with open(output_dir / f'{split}.jsonl', 'w') as f:
        for i in range(count):
            features = [random.random() for _ in range(512)]
            label = random.choice(classes)
            sample = {
                'text': f'Sample {i}',
                'label': label,
                'features': features
            }
            f.write(json.dumps(sample) + '\n')

print(f"Created synthetic dataset: {output_dir}")
EOF
```

2. Create minimal training config:

```bash
cat > quick_test_config.json << 'EOF'
{
  "brain": {
    "name": "quick_test",
    "size": 0,
    "task": 0,
    "num_inputs": 512,
    "num_outputs": 10
  },
  "domains": [
    {"name": "synthetic_test"}
  ],
  "training": {
    "learning_rate": 0.01,
    "lr_decay": 0.95,
    "min_lr": 0.001,
    "batch_size": 16,
    "epochs_per_domain": 2,
    "consolidation_epochs": 1,
    "early_stop_patience": 2,
    "checkpoint_interval": 1
  },
  "datasets_dir": "../datasets/processed",
  "output_dir": "../models/quick_test"
}
EOF
```

3. Train (should take <1 minute):

```bash
python train_foundation_model.py --config quick_test_config.json
```

## Expected Output

### During Training

```
==============================================================
NIMCP Foundation Model Training
==============================================================

Creating brain...
  Name: foundation_model_v1
  Size: 2 (0=TINY, 1=SMALL, 2=MEDIUM, 3=LARGE)
  Task: 0 (0=CLASSIFICATION)
  Inputs: 512
  Outputs: 100

Training on 4 domains:
  1. spam_classification
  2. imdb_sentiment
  3. ag_news
  4. 20newsgroups

==============================================================
Training Domain: spam_classification
==============================================================
Loading data...
  Train samples: 3900
  Val samples: 836

Epoch 1/5
  Train Acc: 0.8234
  Val Acc: 0.8156
  Val Conf: 0.7823
  LR: 0.009500
  Time: 12.34s
  Saving checkpoint: checkpoint_spam_classification_epoch0_20250109_143022.bin
```

### Final Output

```
==============================================================
TRAINING SUMMARY
==============================================================
  spam_classification: 0.9234 accuracy
  imdb_sentiment: 0.8456 accuracy
  ag_news: 0.8123 accuracy
  20newsgroups: 0.7845 accuracy

Final Brain Statistics:
  Neurons: 10,000
  Synapses: 147,234
  Learning steps: 125,000
  Memory: 52.43 MB

Saving final model: ../models/foundation_v1/foundation_model_final.bin
```

## Using Your Trained Model

```python
import nimcp

# Load model
brain = nimcp.Brain.load("../models/foundation_v1/foundation_model_final.bin")

# Make prediction
features = [0.1, 0.2, ...]  # 512-dim feature vector
label, confidence = brain.decide(features)
print(f"Prediction: {label} (confidence: {confidence:.3f})")

# Continue training (fine-tuning)
brain.learn(features, "new_label", confidence=0.9)

# Save fine-tuned model
brain.save("fine_tuned_model.bin")
```

## Troubleshooting

### "nimcp module not found"

```bash
cd /home/bbrelin/nimcp/build
make nimcp_python
export PYTHONPATH=/home/bbrelin/nimcp/build/src/python:$PYTHONPATH
```

### Download fails

- Check internet connection
- Try downloading individual datasets
- Check URL validity in config

### Preprocessing fails

- Check extracted data location
- Verify data format matches config
- Check for corrupted archives

### Training out of memory

Edit `example_training_config.json`:
```json
{
  "brain": {
    "size": 0  // Use TINY instead of MEDIUM
  },
  "training": {
    "batch_size": 16  // Reduce from 32
  }
}
```

### Low accuracy

- Increase brain size
- Increase epochs_per_domain
- Check data quality
- Add more consolidation epochs

## Next Steps

1. **Experiment with hyperparameters:**
   - Brain size (0-3)
   - Learning rate
   - Batch size
   - Number of epochs

2. **Try different datasets:**
   - Add your own datasets
   - Modify preprocessing config
   - Adjust feature dimensions

3. **Fine-tune for specific tasks:**
   - Load foundation model
   - Train on task-specific data
   - Save specialized model

4. **Deploy to production:**
   - Use trained model for inference
   - Create COW clones for parallel processing
   - Monitor performance metrics

## Configuration Tips

### Small/Fast Training (Testing)
- Brain size: 0 (TINY)
- Batch size: 16-32
- Epochs: 2-3
- Features: 256

### Medium Training (Development)
- Brain size: 1 (SMALL)
- Batch size: 32-64
- Epochs: 5-10
- Features: 512

### Large Training (Production)
- Brain size: 2-3 (MEDIUM/LARGE)
- Batch size: 64-128
- Epochs: 10-20
- Features: 512-1024

## File Locations

After running the pipeline:

```
nimcp/
├── datasets/
│   ├── raw/                           # Downloaded files
│   ├── extracted/                     # Extracted datasets
│   └── processed/                     # NIMCP format
│       └── imdb_sentiment/
│           ├── train.jsonl           # Training data
│           ├── val.jsonl             # Validation data
│           ├── test.jsonl            # Test data
│           ├── metadata.json         # Dataset info
│           └── vocab.json            # Vocabulary
│
├── models/
│   └── foundation_v1/
│       ├── checkpoint_*.bin          # Training checkpoints
│       ├── foundation_model_final.bin # Final model
│       ├── training_metrics.json     # All metrics
│       └── foundation_model_stats.json # Model statistics
│
└── scripts/
    ├── download_datasets.py          # Dataset downloader
    ├── preprocess_datasets.py        # Data preprocessor
    ├── train_foundation_model.py     # Training script
    ├── test_trained_model.py         # Model tester
    ├── analyze_training.py           # Results analyzer
    ├── run_full_pipeline.py          # Full pipeline runner
    └── example_*.json                # Example configs
```

## Getting Help

1. Read `README_TRAINING_PIPELINE.md` for detailed documentation
2. Check example config files for reference
3. Use `--help` flag on any script
4. Examine training logs and metrics

## Example Commands Reference

```bash
# Full pipeline
python run_full_pipeline.py --config example_pipeline_config.json

# Skip download (data already downloaded)
python run_full_pipeline.py --config example_pipeline_config.json --skip-download

# Only train (data already prepared)
python run_full_pipeline.py --config example_pipeline_config.json --only-train

# Download single dataset
python download_datasets.py --url URL --name DATASET_NAME

# Preprocess single dataset
python preprocess_datasets.py --dataset NAME --source DIR --format csv

# Train with custom config
python train_foundation_model.py --config custom_config.json

# Resume from checkpoint
python train_foundation_model.py --config config.json --resume checkpoint.bin

# Test model
python test_trained_model.py --model MODEL.bin --dataset DATASET_DIR

# Analyze results
python analyze_training.py --metrics METRICS.json

# Compare multiple runs
python analyze_training.py --compare run1/ run2/ run3/
```

Happy Training!
