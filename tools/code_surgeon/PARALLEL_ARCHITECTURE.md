# Code Surgeon - Parallel Execution Architecture

## Overview

The Code Surgeon parallel execution framework enables distributed test generation, coverage analysis, and test fixing across multiple source files simultaneously. This dramatically reduces execution time for large codebases.

## Architecture Design

### Parent-Child Agent Model

```
                          ┌─────────────────────┐
                          │  Parent Code        │
                          │  Surgeon (Main)     │
                          └──────────┬──────────┘
                                     │
                     ┌───────────────┼───────────────┐
                     │               │               │
              ┌──────▼──────┐ ┌─────▼──────┐ ┌─────▼──────┐
              │  Child       │ │  Child     │ │  Child     │
              │  Worker 1    │ │  Worker 2  │ │  Worker N  │
              └──────┬───────┘ └─────┬──────┘ └─────┬──────┘
                     │               │               │
              ┌──────▼──────┐ ┌─────▼──────┐ ┌─────▼──────┐
              │  Task 1:     │ │  Task 2:   │ │  Task N:   │
              │  p2pnode.c   │ │  security.c│ │  nimcp.c   │
              └──────┬───────┘ └─────┬──────┘ └─────┬──────┘
                     │               │               │
              ┌──────▼──────┐ ┌─────▼──────┐ ┌─────▼──────┐
              │  Result 1    │ │  Result 2  │ │  Result N  │
              └──────┬───────┘ └─────┬──────┘ └─────┬──────┘
                     │               │               │
                     └───────────────┼───────────────┘
                                     │
                          ┌──────────▼──────────┐
                          │  Result Aggregator  │
                          │  (Unified Report)   │
                          └─────────────────────┘
```

### Component Overview

#### 1. **task_queue.py** - Task Management
- **Purpose**: Thread-safe task distribution with priority support
- **Key Classes**:
  - `Task`: Immutable task representation with metadata
  - `TaskQueue`: Priority queue for work distribution
  - `TaskResult`: Mutable result structure for aggregation
  - `TaskPriority`: Priority levels (CRITICAL, HIGH, NORMAL, LOW)
  - `TaskStatus`: Execution status tracking

**Features**:
- Priority-based task execution
- Retry logic for failed tasks
- Thread-safe operations
- Real-time statistics

#### 2. **parallel_executor.py** - Orchestration Engine
- **Purpose**: Manages worker pool and distributes tasks
- **Key Classes**:
  - `ParallelExecutor`: Main orchestration engine
  - `WorkerConfig`: Worker configuration and resources
  - `ExecutionMetrics`: Performance tracking

**Features**:
- ProcessPoolExecutor-based parallelism
- Dynamic worker allocation
- Timeout handling
- Progress monitoring
- Worker-level resource management

#### 3. **result_aggregator.py** - Result Processing
- **Purpose**: Aggregates and formats results from parallel execution
- **Key Classes**:
  - `AggregatedCoverage`: Combined coverage metrics
  - `AggregatedMetrics`: Execution performance metrics
  - `AggregatedReport`: Unified report structure

**Features**:
- Coverage merging
- Multi-format reports (text, JSON, HTML)
- Per-worker breakdowns
- Statistical analysis

#### 4. **code_surgeon.py** - Entry Point (Modified)
- **Purpose**: CLI interface with parallel mode support
- **New Functions**:
  - `orchestrate_parallel_pipeline()`: Parallel execution orchestration
  - Enhanced `main()` with `--parallel` flag

**Features**:
- Backward compatible (serial mode still works)
- New `--parallel N` flag
- Target file specification
- Debug mode support

## Execution Flow

### Serial Mode (Existing Behavior)
```
./code_surgeon.py --mode test-only
```
1. Discover all test binaries
2. Execute tests sequentially
3. Analyze failures
4. Apply fixes iteratively
5. Generate single report

### Parallel Mode (New Feature)
```
./code_surgeon.py --parallel 3 --target-files nimcp_p2pnode.c nimcp_security.c nimcp.c
```

1. **Task Creation Phase**
   - Parent creates Task objects for each target file
   - Tasks enqueued in priority queue
   - Worker pool initialized (N workers)

2. **Distribution Phase**
   - Workers pull tasks from queue
   - Each worker executes Code Surgeon operation independently
   - Tasks run in isolated processes (no shared state)

3. **Execution Phase**
   - Workers generate tests for assigned files
   - Coverage data collected per-worker
   - Results stored in TaskResult objects

4. **Aggregation Phase**
   - Parent collects all TaskResults
   - Coverage data merged (deduplication)
   - Metrics calculated (throughput, success rate)
   - Reports generated (text/JSON/HTML)

5. **Output Phase**
   - Unified report displayed
   - Reports saved to `.code_surgeon/reports/`
   - Exit code reflects overall success

## Data Structures

### Task (Immutable)
```python
@dataclass(frozen=True)
class Task:
    task_id: str              # Unique identifier
    target_file: str          # Source file to process
    test_type: str           # "unit", "integration", etc
    priority: TaskPriority   # Execution priority
    timeout_sec: int         # Timeout for task
    retry_count: int         # Current retry attempt
    max_retries: int         # Maximum retries
    metadata: Dict[str, Any] # Additional context
    created_at: str          # Timestamp
```

### TaskResult (Mutable)
```python
@dataclass
class TaskResult:
    task: Task                    # Original task
    status: TaskStatus            # COMPLETED, FAILED, TIMEOUT
    duration_ms: float            # Execution time
    stdout: str                   # Captured output
    stderr: str                   # Error output
    exit_code: int                # Process exit code
    coverage_data: Dict[str, Any] # Coverage metrics
    tests_created: int            # Number of tests
    tests_passed: int             # Passing tests
    tests_failed: int             # Failing tests
    completed_at: str             # Completion timestamp
    worker_id: str                # Which worker executed
```

### AggregatedReport
```python
@dataclass
class AggregatedReport:
    metrics: AggregatedMetrics     # Performance stats
    coverage: AggregatedCoverage   # Combined coverage
    results: Tuple[TaskResult, ...] # All task results
    timestamp: str                  # Report timestamp
```

## Communication Model

### File-Based (Current Implementation)
- Workers write results to temporary files
- Parent reads and aggregates
- Simple but I/O intensive

### Pipe-Based (Future Enhancement)
- Workers communicate via pipes
- Lower overhead
- Real-time streaming results

### Shared Memory (Advanced Option)
- Fastest communication
- Most complex implementation
- Best for very large result sets

## Performance Characteristics

### Time Complexity
- **Serial Mode**: O(N * T) where N = files, T = time per file
- **Parallel Mode**: O(max(T) * ceil(N / W)) where W = workers
  - Speedup factor: ~W (with perfect parallelism)
  - Real speedup: 0.7W - 0.9W (accounting for overhead)

### Space Complexity
- **Per Worker**: O(1) - isolated processes
- **Parent**: O(N) - stores all results
- **Total**: O(W + N) where W = workers, N = results

### Scalability
- Tested with: 1-16 workers
- Optimal: CPU count or slightly below
- Diminishing returns beyond 16 workers (overhead dominates)

## Resource Management

### Worker Isolation
- Each worker runs in separate process
- No shared state (prevents race conditions)
- Independent memory space
- Crash-safe (one worker failure doesn't affect others)

### Resource Limits (Per Worker)
```python
WorkerConfig(
    worker_id="worker_00",
    max_memory_mb=2048,      # Memory limit
    timeout_sec=600,         # Task timeout
    debug_mode=False,        # Debug output
    coverage_enabled=True    # Coverage collection
)
```

### Timeout Handling
- Per-task timeouts prevent hanging
- Workers killed after timeout
- Tasks marked as TIMEOUT status
- Automatic retry with backoff

## Error Handling

### Task-Level Failures
- Failed tasks marked as FAILED status
- Automatic retry (up to max_retries)
- Error message captured in result
- Parent continues with other tasks

### Worker-Level Failures
- Worker crash doesn't affect other workers
- Task re-queued automatically
- Exception captured and logged
- Graceful degradation

### Parent-Level Failures
- Critical errors halt execution
- Partial results still available
- Cleanup performed before exit

## Usage Examples

### Example 1: Basic Parallel Execution
```bash
cd /home/bbrelin/nimcp
./tools/code_surgeon/code_surgeon.py \
  --parallel 3 \
  --target-files src/networking/p2p/nimcp_p2pnode.c \
                src/security/nimcp_security.c \
                src/api/nimcp.c
```

**Output**:
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

### Example 2: High Parallelism
```bash
./tools/code_surgeon/code_surgeon.py \
  --parallel 8 \
  --target-files src/**/*.c \
  --debug
```

### Example 3: Priority-Based Execution
```python
from task_queue import create_test_generation_task, TaskPriority
from parallel_executor import ParallelExecutor

# Create tasks with priorities
tasks = (
    create_test_generation_task("nimcp.c", priority=TaskPriority.CRITICAL),
    create_test_generation_task("p2pnode.c", priority=TaskPriority.HIGH),
    create_test_generation_task("utils.c", priority=TaskPriority.NORMAL),
)

# Execute
executor = ParallelExecutor(Path.cwd(), num_workers=3)
executor.add_tasks(tasks)
results = executor.execute_parallel()
```

### Example 4: Coverage-Only Mode
```python
from parallel_executor import execute_parallel_coverage_analysis

results = execute_parallel_coverage_analysis(
    target_files=("nimcp_p2pnode.c", "nimcp_security.c"),
    nimcp_root=Path("/home/bbrelin/nimcp"),
    num_workers=2
)
```

## Report Formats

### Text Report (Console)
```
╔══════════════════════════════════════════════════════════════
║ PARALLEL CODE SURGEON - AGGREGATED REPORT
║ Generated: 2025-11-11T10:30:45
╠══════════════════════════════════════════════════════════════
║ EXECUTION METRICS
╠══════════════════════════════════════════════════════════════
║ Total Tasks:          3
║ Completed:            3 (100.0%)
║ Failed:               0 (0.0%)
║ Timeout:              0 (0.0%)
╠══════════════════════════════════════════════════════════════
║ TIMING
╠══════════════════════════════════════════════════════════════
║ Total Duration:       74.04s
║ Average Duration:     24.68s
║ Min Duration:         18.92s
║ Max Duration:         31.67s
║ Throughput:           0.04 tasks/sec
╠══════════════════════════════════════════════════════════════
║ TEST RESULTS
╠══════════════════════════════════════════════════════════════
║ Tests Created:        47
║ Tests Passed:         42
║ Tests Failed:         5
║ Pass Rate:            89.4%
╚══════════════════════════════════════════════════════════════
```

### JSON Report (Machine-Readable)
```json
{
  "timestamp": "2025-11-11T10:30:45",
  "metrics": {
    "total_tasks": 3,
    "completed_tasks": 3,
    "success_rate": 100.0,
    "total_tests_created": 47
  },
  "coverage": {
    "line_coverage_percent": 76.5,
    "branch_coverage_percent": 68.2
  }
}
```

### HTML Report (Interactive)
- Interactive charts and graphs
- Sortable task tables
- Progress bars for coverage
- Per-worker breakdowns
- Saved to `.code_surgeon/reports/report_TIMESTAMP.html`

## Limitations and Considerations

### Current Limitations
1. **No Real-Time Progress**: Workers report only on completion
2. **File-Based Communication**: Higher overhead than pipes
3. **Fixed Worker Count**: Cannot dynamically scale during execution
4. **No Work Stealing**: Workers don't rebalance load

### Design Considerations
1. **Backward Compatibility**: Serial mode unchanged
2. **Isolation**: Workers are completely independent
3. **Simplicity**: Process-based (not thread-based) for safety
4. **Crash Safety**: One worker failure doesn't cascade

### When to Use Parallel Mode
- **Use When**:
  - Processing 3+ independent files
  - Each task takes >10 seconds
  - Have multiple CPU cores available
  - Tasks are CPU-bound (not I/O bound)

- **Don't Use When**:
  - Processing 1-2 files
  - Tasks are very short (<5 seconds)
  - Limited memory available
  - Tasks have dependencies

### Memory Considerations
- Each worker consumes ~2GB RAM
- Parent holds all results in memory
- For 8 workers: ~16GB+ RAM recommended

### Best Practices
1. **Start Conservative**: Begin with num_workers = CPU_count / 2
2. **Monitor Resources**: Use `--debug` to track worker activity
3. **Set Appropriate Timeouts**: Match task complexity
4. **Use Priorities**: Critical tasks run first
5. **Check Reports**: Review HTML report for bottlenecks

## Future Enhancements

### Planned Features
1. **Dynamic Scaling**: Auto-adjust worker count based on load
2. **Work Stealing**: Idle workers take tasks from busy workers
3. **Real-Time Progress**: Streaming results to parent
4. **Distributed Execution**: Run workers on remote machines
5. **Checkpoint/Resume**: Save state and resume interrupted runs
6. **Coverage Deduplication**: Smart merging of overlapping coverage

### Integration Opportunities
1. **CI/CD Integration**: GitHub Actions, Jenkins
2. **Dashboard**: Web UI for monitoring
3. **Metrics Collection**: Prometheus/Grafana
4. **Notification**: Slack/Email on completion

## Configuration

### Environment Variables
```bash
export CODE_SURGEON_MAX_WORKERS=8
export CODE_SURGEON_TIMEOUT=600
export CODE_SURGEON_DEBUG=1
```

### Configuration File (Future)
```yaml
# .code_surgeon.yaml
parallel:
  max_workers: 8
  timeout_sec: 600
  retry_max: 2
  priority_mode: true

coverage:
  enabled: true
  format: lcov
  merge_strategy: union

reports:
  output_dir: .code_surgeon/reports
  formats: [text, json, html]
  retention_days: 30
```

## Testing

### Unit Tests
```bash
cd tools/code_surgeon
python -m pytest test_task_queue.py
python -m pytest test_parallel_executor.py
python -m pytest test_result_aggregator.py
```

### Integration Tests
```bash
# Test with small workload
./code_surgeon.py --parallel 2 --target-files test1.c test2.c

# Test with large workload
./code_surgeon.py --parallel 8 --target-files src/**/*.c
```

### Performance Benchmarks
```bash
# Serial baseline
time ./code_surgeon.py --mode test-only

# Parallel comparison
time ./code_surgeon.py --parallel 4 --target-files src/**/*.c
```

## Troubleshooting

### Workers Not Starting
- Check CPU availability
- Verify Python multiprocessing support
- Check for resource limits (ulimit)

### Tasks Timing Out
- Increase timeout: `--timeout 900`
- Reduce worker count (memory pressure)
- Check for infinite loops in tests

### High Memory Usage
- Reduce worker count
- Enable result streaming (future feature)
- Process results incrementally

### Inconsistent Results
- Check for shared state bugs
- Verify task isolation
- Review worker logs

## Contact & Support

For questions or issues with parallel execution:
- Review logs in `.code_surgeon/logs/`
- Check HTML report for detailed diagnostics
- Enable `--debug` for verbose output
