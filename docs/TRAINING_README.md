# NIMCP Training Pipeline Documentation

## Overview

NIMCP provides three training approaches for foundation models:

1. **Traditional Download** (`download_foundation_datasets.py`) - Download complete datasets
2. **Hybrid Streaming** (`hybrid_train.py`) - Smart training with streaming for massive datasets
3. **Parallel Training** (`parallel_train.py`) - Multi-process parallel batch training (RECOMMENDED)

## Parallel Training Pipeline (RECOMMENDED)

The parallel training pipeline maximizes throughput using multi-process batch training:

### Key Features
- ✅ **Parallel Batch Processing** - Trains multiple batches simultaneously
- ✅ **Scans Existing Datasets** - Automatically finds downloaded datasets
- ✅ **Trains on Local Datasets First** - Processes what you already have
- ✅ **Automatic Cleanup** - Deletes datasets after training to free space
- ✅ **Batch Downloads** - Downloads N datasets at a time
- ✅ **Multi-Core Utilization** - Uses all available CPU cores
- ✅ **Checkpoint/Resume** - Save progress for recovery

### Quick Start

```bash
# Basic usage - train on existing datasets, then download new ones
python3 scripts/parallel_train.py

# Custom number of workers (default: 4)
python3 scripts/parallel_train.py --num-workers 8

# Larger batches for faster processing
python3 scripts/parallel_train.py --batch-size 2000 --num-workers 8

# Download 5 datasets at once
python3 scripts/parallel_train.py --download-batch-size 5

# Test with limited examples
python3 scripts/parallel_train.py --max-examples 10000

# Auto-cleanup without prompts
python3 scripts/parallel_train.py --auto-cleanup
```

### How It Works

#### Two-Phase Parallel Processing

**Phase 1: Train on Existing Datasets (Parallel)**
1. Scans `datasets/foundation/` directory
2. Finds all datasets with `metadata.json`
3. Loads each dataset in batches
4. Distributes batches across worker processes
5. Workers train in parallel
6. After training completes, deletes dataset to free space
7. Repeats for all existing datasets

**Phase 2: Download and Train New Datasets (Batch)**
1. Downloads N datasets in parallel (configurable)
2. Once downloaded, trains on them in parallel
3. Deletes after training
4. Downloads next batch
5. Repeats until all datasets processed

#### Worker Architecture

```
Main Process
    ├── Worker 1: Processing batch 1
    ├── Worker 2: Processing batch 2
    ├── Worker 3: Processing batch 3
    └── Worker 4: Processing batch 4

Each worker:
- Receives a batch of examples
- Trains the model on the batch
- Returns training metrics
- Gets next batch
```

### Configuration Options

```bash
--batch-size N              # Examples per training batch (default: 1000)
--num-workers N             # Number of parallel workers (default: 4)
--checkpoint-interval N     # Save checkpoint every N examples (default: 10000)
--max-examples N            # Limit examples per dataset (for testing)
--no-cleanup                # Keep datasets after training
--auto-cleanup              # Auto-confirm deletion (no prompts)
--download-batch-size N     # Download N datasets at once (default: 3)
```

### Example Workflows

#### Workflow 1: Maximum Throughput

```bash
# Use all CPU cores for maximum speed
python3 scripts/parallel_train.py \
  --num-workers 16 \
  --batch-size 2000 \
  --download-batch-size 5 \
  --auto-cleanup
```

#### Workflow 2: Conservative Resource Usage

```bash
# Use fewer workers, smaller batches
python3 scripts/parallel_train.py \
  --num-workers 2 \
  --batch-size 500 \
  --download-batch-size 2
```

#### Workflow 3: Quick Test

```bash
# Test with limited examples per dataset
python3 scripts/parallel_train.py \
  --max-examples 1000 \
  --num-workers 4 \
  --auto-cleanup
```

### Performance Comparison

| Mode | Throughput | Memory | Disk Usage | Use Case |
|------|-----------|---------|------------|----------|
| **Parallel** | ~4000 ex/sec | 2-4GB | Low (cleanup) | Production, fast training |
| Hybrid | ~1000 ex/sec | 2-4GB | Low (cleanup) | Single-core systems |
| Traditional | ~500 ex/sec | 4-8GB | High | Small datasets |

**Parallel mode is ~4x faster** than single-threaded approaches!

## Hybrid Training Pipeline

The hybrid pipeline automatically:
- ✅ Uses already-downloaded datasets (no re-download!)
- ✅ Trains on local datasets first, then deletes them to free disk space
- ✅ Streams massive datasets (>10GB) in batches
- ✅ Saves checkpoints for resume capability
- ✅ Cleans up temporary files automatically
- ✅ Handles interruptions gracefully

### Quick Start

```bash
# Basic usage - train on all datasets
python3 scripts/hybrid_train.py

# Train with custom batch size
python3 scripts/hybrid_train.py --batch-size 500

# Limit to 10,000 examples per dataset (for testing)
python3 scripts/hybrid_train.py --max-examples 10000

# Train only specific datasets
python3 scripts/hybrid_train.py --datasets wikitext103,squad_v2,commonsense_qa

# Resume from last checkpoint
python3 scripts/hybrid_train.py --resume

# Stream threshold: only stream datasets >50GB
python3 scripts/hybrid_train.py --stream-threshold 50.0
```

### How It Works

#### Dataset Classification

The pipeline classifies each dataset:

| Dataset | Size | Method | Rationale |
|---------|------|--------|-----------|
| wikitext103 | ~500MB | **Local** | Already downloaded |
| squad_v2 | ~100MB | **Local** | Already downloaded |
| github_code_2025 | ~150GB | **Stream** | Too large to download |
| the_stack | 6TB | **Stream** | Massive - batch processing |
| github_code | 1TB | **Stream** | Very large dataset |

#### Streaming Process

For massive datasets, the pipeline:

1. **Stream batch** (1000 examples) → No full download!
2. **Extract features** → Convert to numeric inputs
3. **Train brain** → Incremental learning
4. **Save checkpoint** → Resume capability
5. **Cleanup** → Free disk space
6. **Repeat** → Until dataset complete

#### Two-Phase Training Strategy

The pipeline intelligently manages disk space using a two-phase approach:

**Phase 1: Local Datasets (Train → Delete)**
- Trains on already-downloaded datasets first
- After training completes, deletes the dataset directory
- Frees disk space for streaming new datasets
- Prompts for confirmation before deletion (unless `--auto-cleanup` used)

**Phase 2: Remote Datasets (Stream)**
- Streams massive datasets in batches
- Never downloads full dataset
- Processes incrementally with minimal disk usage

**Example workflow:**
```
Phase 1: Local Datasets
  ✓ Training on wikitext103 (500 MB)
  🗑️  Delete dataset to free 500 MB? (y/N): y
  ✓ Deleted wikitext103 - freed 512.3 MB

  ✓ Training on squad_v2 (100 MB)
  🗑️  Delete dataset to free 100 MB? (y/N): y
  ✓ Deleted squad_v2 - freed 98.7 MB

Phase 2: Remote Datasets (Streaming)
  🔄 Streaming github_code_2025 (150 GB)
  📊 Progress: 10000 examples (125.3 ex/sec)
  ...
```

**To keep local datasets:**
```bash
python3 scripts/hybrid_train.py --no-cleanup
```

**To auto-confirm deletions:**
```bash
python3 scripts/hybrid_train.py --auto-cleanup
```

### Configuration Options

```bash
--batch-size N              # Examples per training batch (default: 1000)
--checkpoint-interval N     # Save checkpoint every N examples (default: 10000)
--stream-threshold N        # Stream datasets > N GB (default: 10.0)
--max-examples N            # Limit examples per dataset (for testing)
--datasets name1,name2      # Train only specific datasets
--resume                    # Resume from last checkpoint
--no-cleanup                # Keep local datasets after training (don't delete)
--auto-cleanup              # Auto-confirm dataset deletion (no prompts)
```

### Directory Structure

```
nimcp/
├── datasets/foundation/          # Downloaded datasets
│   ├── wikitext103/              # Local dataset
│   │   ├── train.jsonl
│   │   └── metadata.json
│   └── squad_v2/                 # Local dataset
│       └── ...
├── checkpoints/                  # Training checkpoints
│   ├── wikitext103_progress.json
│   └── brain_wikitext103.checkpoint
└── scripts/
    ├── hybrid_train.py           # Hybrid training (recommended)
    ├── download_foundation_datasets.py  # Traditional download
    └── foundation_datasets_config.json
```

### Progress Tracking

Each dataset creates a progress file:

```json
{
  "dataset_name": "github_code_2025",
  "examples_processed": 50000,
  "batches_completed": 50,
  "last_checkpoint": 50000,
  "completed": false,
  "timestamp": 1699564800
}
```

**Resume from checkpoint:**
```bash
python3 scripts/hybrid_train.py --resume
```

### Memory Management

The pipeline automatically:
- Processes data in small batches (configurable)
- Deletes temporary files after each batch
- Runs garbage collection periodically
- Never loads full datasets into memory

**Typical memory usage:** ~2-4GB regardless of dataset size!

## Dataset Priorities

The pipeline processes datasets in curriculum order:

1. **Language** (foundation)
2. **Programming** (code understanding)
3. **Ethics** (moral reasoning)
4. **Philosophy** (abstract thinking)
5. **History** (temporal understanding)
6. **Psychology** (human behavior)
7. **Anthropology** (cultural knowledge)
8. **Biology** (life sciences)
9. **Chemistry** (molecular sciences)
10. **Physics** (physical laws)

## Handling Interruptions

The pipeline handles interruptions gracefully:

```bash
# Start training
python3 scripts/hybrid_train.py

# Press Ctrl+C to interrupt
^C
⏸ Interrupted - saving checkpoint...
💾 Checkpoint saved

# Resume later
python3 scripts/hybrid_train.py --resume
↻ Resuming from checkpoint: 50000 examples processed
```

## Example Workflows

### Workflow 1: Quick Test Run

```bash
# Test with limited examples
python3 scripts/hybrid_train.py \
  --max-examples 1000 \
  --datasets wikitext103,squad_v2
```

### Workflow 2: Full Training Run

```bash
# Train on all available datasets
python3 scripts/hybrid_train.py \
  --batch-size 1000 \
  --checkpoint-interval 10000
```

### Workflow 3: Large Dataset Streaming

```bash
# Focus on massive programming datasets
python3 scripts/hybrid_train.py \
  --datasets github_code_2025,the_stack \
  --stream-threshold 100.0 \
  --batch-size 500
```

### Workflow 4: Resume Long-Running Training

```bash
# Start training
python3 scripts/hybrid_train.py

# ... runs for hours, interrupted ...

# Resume from checkpoint
python3 scripts/hybrid_train.py --resume
```

### Workflow 5: Disk Space Management

```bash
# Default behavior: train local datasets, then delete after confirmation
python3 scripts/hybrid_train.py

# Keep local datasets (don't delete after training)
python3 scripts/hybrid_train.py --no-cleanup

# Auto-delete without prompts (for automation)
python3 scripts/hybrid_train.py --auto-cleanup

# Train local datasets with cleanup, then stream massive datasets
python3 scripts/hybrid_train.py \
  --auto-cleanup \
  --stream-threshold 10.0
```

## Traditional Download (Old Method)

For small datasets, you can still use traditional download:

```bash
# Download all datasets (WARNING: Very large!)
python3 scripts/download_foundation_datasets.py

# This will try to download:
# - Small datasets (~1GB total) ✓
# - Medium datasets (~10GB total) ✓
# - Large datasets (~150GB+) ⚠️ May take hours/days
# - Massive datasets (6TB+) ❌ Not recommended
```

## Troubleshooting

### "Dataset already downloading"

**Solution:** Use hybrid mode - it checks for existing downloads

```bash
python3 scripts/hybrid_train.py
```

### "Out of disk space"

**Solution:** Use streaming with lower stream threshold

```bash
python3 scripts/hybrid_train.py --stream-threshold 5.0
```

### "Training too slow"

**Solution:** Increase batch size

```bash
python3 scripts/hybrid_train.py --batch-size 2000
```

### "Want to test quickly"

**Solution:** Use max-examples limit

```bash
python3 scripts/hybrid_train.py --max-examples 1000
```

### "Need to free disk space"

**Solution:** Use default cleanup behavior or auto-cleanup

```bash
# Default: prompts for confirmation before deleting
python3 scripts/hybrid_train.py

# Auto-delete without prompts
python3 scripts/hybrid_train.py --auto-cleanup
```

### "Want to keep datasets after training"

**Solution:** Use --no-cleanup flag

```bash
python3 scripts/hybrid_train.py --no-cleanup
```

## Estimated Training Times

| Dataset | Size | Method | Time (1000 batch) |
|---------|------|--------|-------------------|
| wikitext103 | 1.8M examples | Local | ~30 min |
| squad_v2 | 130K examples | Local | ~5 min |
| github_code_2025 | ~5M examples | Stream | ~8 hours |
| the_stack | ~200M files | Stream | ~2-3 weeks |

**Note:** Times assume GPU acceleration and 1000 ex/sec throughput.

## Summary

### Choose Your Training Pipeline

#### 🚀 Parallel Training (RECOMMENDED)
**Best for:** Production use, multi-core systems, maximum speed

```bash
python3 scripts/parallel_train.py --num-workers 8 --auto-cleanup
```

✅ **~4x faster** than single-threaded
✅ **Multi-core utilization** - Uses all CPU cores
✅ **Parallel batch processing** - Process multiple batches simultaneously
✅ **Automatic cleanup** - Trains on existing datasets, deletes them, downloads new ones
✅ **Batch downloads** - Download N datasets in parallel
✅ **Memory efficient** - 2-4GB regardless of dataset size

#### ⚡ Hybrid Training
**Best for:** Single-core systems, custom streaming needs

```bash
python3 scripts/hybrid_train.py --auto-cleanup
```

✅ **Smart, efficient, resumable**
✅ **Automatic disk space management**
✅ **Streams massive datasets** - Handles 6TB+ datasets
✅ **Memory efficient** - 2-4GB usage

#### 📦 Traditional Download
**Best for:** Small datasets, one-time downloads

```bash
python3 scripts/download_foundation_datasets.py
```

⚠️ **Warning:** Will download entire datasets (can be 6TB+)
