# Code Surgeon Parallel Execution - Quick Start Guide

## Installation

No additional dependencies required. The parallel execution framework uses Python's built-in `multiprocessing` module.

## Basic Usage

### 1. Serial Mode (Existing Behavior)
```bash
cd /home/bbrelin/nimcp
./tools/code_surgeon/code_surgeon.py --mode test-only
```

### 2. Parallel Mode (New Feature)
```bash
./tools/code_surgeon/code_surgeon.py \
  --parallel 3 \
  --target-files \
    src/networking/p2p/nimcp_p2pnode.c \
    src/security/nimcp_security.c \
    src/api/nimcp.c
```

## Command-Line Options

### New Flags
- `--parallel N` - Enable parallel execution with N workers
  - `N > 0`: Use N workers
  - `N = 0`: Auto-detect (uses CPU count)
  - Omit flag: Use serial mode (backward compatible)

- `--target-files FILE1 FILE2 ...` - Specify files to process
  - Required when using `--parallel`
  - Paths relative to project root
  - Can use shell globbing: `src/**/*.c`

- `--debug` - Enable debug output
  - Shows worker activity
  - Displays task assignments
  - Useful for troubleshooting

### Existing Flags (Still Work)
- `--mode {test-only,full}` - Execution mode
- `--max-iterations N` - Max fix iterations
- `--coverage` / `--no-coverage` - Coverage analysis

## Quick Examples

### Example 1: Generate Tests for 3 Files in Parallel
```bash
./tools/code_surgeon/code_surgeon.py \
  --parallel 3 \
  --target-files \
    src/networking/p2p/nimcp_p2pnode.c \
    src/security/nimcp_security.c \
    src/api/nimcp.c
```

**Expected Output:**
```
============================================================
PARALLEL EXECUTION STARTED
============================================================
Workers: 3
Tasks: 3
============================================================

[✓] nimcp_p2pnode.c (23.45s) - worker_00
[✓] nimcp_security.c (18.92s) - worker_01
[✓] nimcp.c (31.67s) - worker_02

============================================================
PARALLEL EXECUTION COMPLETED
============================================================
Duration: 31.67s
Throughput: 0.09 tasks/sec
Completed: 3/3
Failed: 0/3
============================================================
```

### Example 2: Auto-Detect Worker Count
```bash
./tools/code_surgeon/code_surgeon.py \
  --parallel 0 \
  --target-files src/networking/**/*.c
```

### Example 3: Debug Mode
```bash
./tools/code_surgeon/code_surgeon.py \
  --parallel 2 \
  --target-files file1.c file2.c \
  --debug
```

### Example 4: Use Shell Script
```bash
./tools/code_surgeon/example_parallel_usage.sh
# Interactive menu will appear
```

## Python API

### Programmatic Usage
```python
from pathlib import Path
from parallel_executor import execute_parallel_test_generation
from result_aggregator import aggregate_results, generate_text_report

# Execute parallel test generation
nimcp_root = Path("/home/bbrelin/nimcp")
target_files = (
    "src/networking/p2p/nimcp_p2pnode.c",
    "src/security/nimcp_security.c",
    "src/api/nimcp.c"
)

results = execute_parallel_test_generation(
    target_files=target_files,
    nimcp_root=nimcp_root,
    num_workers=3,
    debug_mode=False
)

# Aggregate results
report = aggregate_results(results)
print(generate_text_report(report))
```

### Advanced Usage with Task Priorities
```python
from task_queue import create_test_generation_task, TaskPriority
from parallel_executor import ParallelExecutor

# Create tasks with different priorities
tasks = (
    create_test_generation_task("nimcp.c", priority=TaskPriority.CRITICAL),
    create_test_generation_task("p2pnode.c", priority=TaskPriority.HIGH),
    create_test_generation_task("utils.c", priority=TaskPriority.NORMAL),
)

# Execute with priority ordering
executor = ParallelExecutor(Path.cwd(), num_workers=3)
executor.add_tasks(tasks)
results = executor.execute_parallel()
```

## Output Reports

Reports are automatically saved to `.code_surgeon/reports/` in three formats:

### 1. Text Report (`report_TIMESTAMP.txt`)
Console-friendly format with ASCII box drawing.

### 2. JSON Report (`report_TIMESTAMP.json`)
Machine-readable format for post-processing:
```json
{
  "timestamp": "2025-11-11T10:30:45",
  "metrics": {
    "total_tasks": 3,
    "completed_tasks": 3,
    "success_rate": 100.0
  },
  "coverage": {
    "line_coverage_percent": 76.5
  }
}
```

### 3. HTML Report (`report_TIMESTAMP.html`)
Interactive report with charts and graphs. Open in browser:
```bash
# Find latest report
LATEST=$(ls -t .code_surgeon/reports/*.html | head -1)

# Open in browser (Linux)
xdg-open "$LATEST"

# Or manually
firefox "$LATEST"
```

## Performance Comparison

### Serial Execution (Baseline)
```bash
time ./tools/code_surgeon/code_surgeon.py --mode test-only
# Expected: ~90s for 3 files (30s each)
```

### Parallel Execution (3 Workers)
```bash
time ./tools/code_surgeon/code_surgeon.py \
  --parallel 3 \
  --target-files file1.c file2.c file3.c
# Expected: ~30s (tasks run simultaneously)
# Speedup: ~3x
```

## Troubleshooting

### Problem: "Error: --target-files required"
**Solution**: When using `--parallel`, you must specify target files:
```bash
# Wrong
./code_surgeon.py --parallel 3

# Correct
./code_surgeon.py --parallel 3 --target-files file1.c file2.c
```

### Problem: Workers not starting
**Solution**: Check system resources:
```bash
# Check CPU count
nproc

# Check available memory
free -h

# Try fewer workers
./code_surgeon.py --parallel 2 --target-files file1.c file2.c
```

### Problem: Tasks timing out
**Solution**: Increase timeout or reduce parallelism:
```bash
# Edit worker config in parallel_executor.py
# Or reduce worker count to reduce memory pressure
./code_surgeon.py --parallel 2 --target-files file1.c file2.c file3.c
```

### Problem: High memory usage
**Solution**: Reduce worker count:
```bash
# Each worker uses ~2GB RAM
# For 8GB system, use max 3 workers
./code_surgeon.py --parallel 3 --target-files ...
```

## Best Practices

### 1. Start Conservative
```bash
# First try with 2 workers
./code_surgeon.py --parallel 2 --target-files file1.c file2.c file3.c

# Monitor system resources
htop  # In another terminal

# Gradually increase if system handles it well
./code_surgeon.py --parallel 4 --target-files ...
```

### 2. Use Appropriate Worker Count
```bash
# Rule of thumb: num_workers ≤ CPU_count
# For CPU-bound tasks: num_workers = CPU_count
# For I/O-bound tasks: num_workers = CPU_count * 2

# Check CPU count
nproc
# Returns: 8 (for example)

# Use 4-6 workers for CPU-bound test generation
./code_surgeon.py --parallel 6 --target-files ...
```

### 3. Group Similar Tasks
```bash
# Group files by module for better caching
./code_surgeon.py --parallel 3 --target-files \
  src/networking/p2p/*.c \
  src/networking/protocol/*.c \
  src/networking/events/*.c
```

### 4. Monitor Progress
```bash
# Use debug mode to see real-time progress
./code_surgeon.py --parallel 4 --target-files ... --debug

# Check reports directory
watch -n 5 'ls -lh .code_surgeon/reports/'
```

## When to Use Parallel Mode

### Use Parallel When:
- Processing 3+ independent files
- Each task takes >10 seconds
- Have multiple CPU cores (4+)
- Tasks are CPU-bound (test generation, analysis)

### Use Serial When:
- Processing 1-2 files
- Tasks are very short (<5 seconds)
- Limited memory (<4GB available)
- Tasks have dependencies

## Resource Requirements

### Minimum
- 2 CPU cores
- 4GB RAM
- 1GB free disk space

### Recommended
- 4-8 CPU cores
- 8-16GB RAM
- 5GB free disk space

### Optimal
- 8+ CPU cores
- 16GB+ RAM
- SSD storage

## Next Steps

1. **Read Full Documentation**: See `PARALLEL_ARCHITECTURE.md`
2. **Try Examples**: Run `./example_parallel_usage.sh`
3. **View Reports**: Check `.code_surgeon/reports/` after execution
4. **Customize**: Edit task priorities and worker configs as needed

## Support

For issues or questions:
- Check logs: `.code_surgeon/logs/`
- Review HTML reports for diagnostics
- Enable `--debug` for verbose output
- See `PARALLEL_ARCHITECTURE.md` for detailed architecture
