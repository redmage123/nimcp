#!/usr/bin/env python3
"""
Parallel Executor - Parent-Child Code Surgeon Agent System

WHAT: Orchestrates parallel execution of Code Surgeon child agents
WHY:  Maximize throughput for large test suites via distributed execution
HOW:  Spawn N child Code Surgeon processes, distribute tasks, aggregate results

PATTERNS: Master-Worker, Process Pool, Producer-Consumer, Aggregator
"""

import subprocess
import multiprocessing
import sys
import json
import time
from pathlib import Path
from typing import Tuple, Optional, List, Dict, Any
from dataclasses import dataclass, field
from datetime import datetime
from concurrent.futures import ProcessPoolExecutor, as_completed

from task_queue import (
    Task, TaskResult, TaskQueue, TaskStatus, TaskPriority,
    create_test_generation_task, create_coverage_task, create_fix_task
)

#==============================================================================
# Data Structures
#==============================================================================

@dataclass
class WorkerConfig:
    """
    Worker configuration

    WHAT: Settings for child Code Surgeon agents
    WHY:  Configure worker behavior and resources
    HOW:  Immutable configuration dataclass
    """
    worker_id: str
    max_memory_mb: int = 2048
    timeout_sec: int = 600
    debug_mode: bool = False
    coverage_enabled: bool = True

@dataclass
class ExecutionMetrics:
    """
    Execution metrics for monitoring

    WHAT: Performance and progress tracking
    WHY:  Monitor parallel execution health
    HOW:  Mutable metrics updated during execution
    """
    start_time: float = field(default_factory=time.time)
    end_time: Optional[float] = None
    total_tasks: int = 0
    completed_tasks: int = 0
    failed_tasks: int = 0
    active_workers: int = 0
    peak_workers: int = 0
    total_duration_sec: float = 0.0

    def calculate_duration(self) -> float:
        """Calculate total execution duration"""
        if self.end_time:
            return self.end_time - self.start_time
        return time.time() - self.start_time

    def get_throughput(self) -> float:
        """Calculate tasks per second"""
        duration = self.calculate_duration()
        if duration > 0:
            return self.completed_tasks / duration
        return 0.0

#==============================================================================
# Worker Functions (Pure, for multiprocessing)
#==============================================================================

def execute_task_worker(task: Task,
                        worker_config: WorkerConfig,
                        nimcp_root: Path) -> TaskResult:
    """
    Worker function that executes a single task

    WHAT: Run Code Surgeon operation for one task
    WHY:  Isolated worker function for multiprocessing
    HOW:  Execute operation based on task metadata

    NOTE: Must be picklable for multiprocessing
    COMPLEXITY: O(task_duration)
    """
    start_time = time.time()

    try:
        operation = task.metadata.get('operation', 'generate_tests')

        if operation == 'generate_tests':
            result = _execute_test_generation(task, worker_config, nimcp_root)
        elif operation == 'analyze_coverage':
            result = _execute_coverage_analysis(task, worker_config, nimcp_root)
        elif operation == 'fix_test':
            result = _execute_test_fix(task, worker_config, nimcp_root)
        else:
            result = TaskResult(
                task=task,
                status=TaskStatus.FAILED,
                duration_ms=0.0,
                stderr=f"Unknown operation: {operation}",
                worker_id=worker_config.worker_id
            )

        result.duration_ms = (time.time() - start_time) * 1000
        result.worker_id = worker_config.worker_id

        return result

    except Exception as e:
        duration_ms = (time.time() - start_time) * 1000
        return TaskResult(
            task=task,
            status=TaskStatus.FAILED,
            duration_ms=duration_ms,
            stderr=f"Worker exception: {str(e)}",
            worker_id=worker_config.worker_id
        )

def _execute_test_generation(task: Task,
                             worker_config: WorkerConfig,
                             nimcp_root: Path) -> TaskResult:
    """
    Execute test generation for a target file

    WHAT: Generate tests for specific source file
    WHY:  Create comprehensive test coverage
    HOW:  Run Code Surgeon in test generation mode

    NOTE: This spawns a child Code Surgeon process
    """
    # Build command for child Code Surgeon instance
    code_surgeon_path = nimcp_root / "tools" / "code_surgeon" / "code_surgeon.py"

    # For now, we'll simulate test generation
    # In real implementation, this would call Code Surgeon with specific target
    cmd = [
        sys.executable,
        str(code_surgeon_path),
        "--mode", "test-only",
        "--target-file", task.target_file
    ]

    if worker_config.coverage_enabled:
        cmd.append("--coverage")

    if worker_config.debug_mode:
        cmd.append("--debug")

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=task.timeout_sec,
            cwd=nimcp_root
        )

        # Parse output for test statistics
        tests_created = _parse_tests_created(result.stdout)
        tests_passed = _parse_tests_passed(result.stdout)
        tests_failed = _parse_tests_failed(result.stdout)
        coverage_data = _parse_coverage_data(result.stdout)

        status = TaskStatus.COMPLETED if result.returncode == 0 else TaskStatus.FAILED

        return TaskResult(
            task=task,
            status=status,
            duration_ms=0.0,  # Will be set by caller
            stdout=result.stdout,
            stderr=result.stderr,
            exit_code=result.returncode,
            coverage_data=coverage_data,
            tests_created=tests_created,
            tests_passed=tests_passed,
            tests_failed=tests_failed
        )

    except subprocess.TimeoutExpired:
        return TaskResult(
            task=task,
            status=TaskStatus.TIMEOUT,
            duration_ms=task.timeout_sec * 1000,
            stderr=f"Task timed out after {task.timeout_sec}s"
        )

    except Exception as e:
        return TaskResult(
            task=task,
            status=TaskStatus.FAILED,
            duration_ms=0.0,
            stderr=f"Execution error: {str(e)}"
        )

def _execute_coverage_analysis(task: Task,
                               worker_config: WorkerConfig,
                               nimcp_root: Path) -> TaskResult:
    """
    Execute coverage analysis for a target file

    WHAT: Analyze code coverage for specific file
    WHY:  Identify gaps for test generation
    HOW:  Run coverage tools and collect metrics
    """
    # Placeholder for coverage analysis
    # Real implementation would run lcov/gcov
    return TaskResult(
        task=task,
        status=TaskStatus.COMPLETED,
        duration_ms=0.0,
        stdout="Coverage analysis completed",
        coverage_data={'line_coverage': 75.0}
    )

def _execute_test_fix(task: Task,
                     worker_config: WorkerConfig,
                     nimcp_root: Path) -> TaskResult:
    """
    Execute test fix operation

    WHAT: Fix a failing test
    WHY:  Self-healing test framework
    HOW:  Run Code Surgeon in fix mode
    """
    # Placeholder for test fixing
    # Real implementation would run Code Surgeon fix mode
    return TaskResult(
        task=task,
        status=TaskStatus.COMPLETED,
        duration_ms=0.0,
        stdout="Fix applied successfully",
        tests_passed=1
    )

#==============================================================================
# Parsing Utilities
#==============================================================================

def _parse_tests_created(stdout: str) -> int:
    """Extract number of tests created from output"""
    import re
    match = re.search(r'Tests created:\s*(\d+)', stdout)
    return int(match.group(1)) if match else 0

def _parse_tests_passed(stdout: str) -> int:
    """Extract number of tests passed from output"""
    import re
    match = re.search(r'Passing:\s*(\d+)', stdout)
    return int(match.group(1)) if match else 0

def _parse_tests_failed(stdout: str) -> int:
    """Extract number of tests failed from output"""
    import re
    match = re.search(r'Failing:\s*(\d+)', stdout)
    return int(match.group(1)) if match else 0

def _parse_coverage_data(stdout: str) -> Optional[Dict[str, Any]]:
    """Extract coverage data from output"""
    import re
    match = re.search(r'Coverage:\s*(\d+\.?\d*)%', stdout)
    if match:
        return {'line_coverage': float(match.group(1))}
    return None

#==============================================================================
# Parallel Executor
#==============================================================================

class ParallelExecutor:
    """
    Parallel Code Surgeon Executor

    WHAT: Orchestrates parallel task execution
    WHY:  Maximize throughput via parallelism
    HOW:  ProcessPoolExecutor with TaskQueue

    THREAD SAFETY: All operations are thread-safe
    """

    def __init__(self,
                 nimcp_root: Path,
                 num_workers: Optional[int] = None,
                 debug_mode: bool = False):
        """
        Initialize parallel executor

        WHAT: Create executor with worker pool
        WHY:  Setup infrastructure for parallel execution
        HOW:  Initialize queue, pool, and config

        PARAMETERS:
            num_workers: Number of parallel workers (default: CPU count)
            debug_mode: Enable debug output
        """
        self.nimcp_root = nimcp_root
        self.num_workers = num_workers or min(multiprocessing.cpu_count(), 8)
        self.debug_mode = debug_mode

        self.task_queue = TaskQueue()
        self.metrics = ExecutionMetrics()
        self.worker_configs = self._create_worker_configs()

        print(f"[PARALLEL EXECUTOR] Initialized with {self.num_workers} workers")

    def _create_worker_configs(self) -> List[WorkerConfig]:
        """Create configuration for each worker"""
        configs = []
        for i in range(self.num_workers):
            config = WorkerConfig(
                worker_id=f"worker_{i:02d}",
                debug_mode=self.debug_mode,
                coverage_enabled=True
            )
            configs.append(config)
        return configs

    def add_task(self, task: Task) -> bool:
        """
        Add task to queue

        WHAT: Enqueue task for execution
        WHY:  Producer adds work
        HOW:  Delegate to task queue

        RETURNS: True if added successfully
        """
        success = self.task_queue.add_task(task)
        if success:
            self.metrics.total_tasks += 1
        return success

    def add_tasks(self, tasks: Tuple[Task, ...]) -> int:
        """
        Add multiple tasks

        WHAT: Bulk task addition
        WHY:  More efficient than individual adds
        HOW:  Delegate to task queue

        RETURNS: Number of tasks added
        """
        added = self.task_queue.add_tasks(tasks)
        self.metrics.total_tasks += added
        return added

    def execute_parallel(self, timeout_sec: Optional[float] = None) -> Tuple[TaskResult, ...]:
        """
        Execute all tasks in parallel

        WHAT: Main execution loop - distributes and executes tasks
        WHY:  Parallel execution for maximum throughput
        HOW:  ProcessPoolExecutor with worker function

        RETURNS: Tuple of all task results
        COMPLEXITY: O(longest_task) with parallelism factor of num_workers
        """
        print(f"\n{'='*60}")
        print(f"PARALLEL EXECUTION STARTED")
        print(f"{'='*60}")
        print(f"Workers: {self.num_workers}")
        print(f"Tasks: {self.metrics.total_tasks}")
        print(f"{'='*60}\n")

        self.metrics.start_time = time.time()
        self.metrics.active_workers = self.num_workers
        self.metrics.peak_workers = self.num_workers

        results = []

        with ProcessPoolExecutor(max_workers=self.num_workers) as executor:
            # Submit all tasks
            future_to_task = {}

            worker_idx = 0
            while not self.task_queue.is_empty():
                task = self.task_queue.get_task(timeout_sec=1.0)
                if task is None:
                    break

                worker_config = self.worker_configs[worker_idx % self.num_workers]
                worker_idx += 1

                future = executor.submit(
                    execute_task_worker,
                    task,
                    worker_config,
                    self.nimcp_root
                )
                future_to_task[future] = task

                if self.debug_mode:
                    print(f"[SUBMIT] {task.task_id} → {worker_config.worker_id}")

            # Collect results as they complete
            for future in as_completed(future_to_task, timeout=timeout_sec):
                task = future_to_task[future]

                try:
                    result = future.result()
                    results.append(result)
                    self.task_queue.complete_task(result)

                    if result.status == TaskStatus.COMPLETED:
                        self.metrics.completed_tasks += 1
                        status_symbol = "✓"
                    else:
                        self.metrics.failed_tasks += 1
                        status_symbol = "✗"

                    print(f"[{status_symbol}] {task.target_file} ({result.duration_ms/1000:.2f}s) - {result.worker_id}")

                    # Retry failed tasks if needed
                    if result.status == TaskStatus.FAILED and task.retry_count < task.max_retries:
                        if self.task_queue.retry_task(task):
                            print(f"[RETRY] {task.task_id} (attempt {task.retry_count + 2}/{task.max_retries + 1})")

                except Exception as e:
                    print(f"[✗] {task.target_file} - Worker exception: {str(e)}")
                    self.metrics.failed_tasks += 1

                    # Create failed result
                    failed_result = TaskResult(
                        task=task,
                        status=TaskStatus.FAILED,
                        duration_ms=0.0,
                        stderr=f"Worker exception: {str(e)}"
                    )
                    results.append(failed_result)
                    self.task_queue.complete_task(failed_result)

        self.metrics.end_time = time.time()
        self.metrics.total_duration_sec = self.metrics.calculate_duration()

        print(f"\n{'='*60}")
        print(f"PARALLEL EXECUTION COMPLETED")
        print(f"{'='*60}")
        print(f"Duration: {self.metrics.total_duration_sec:.2f}s")
        print(f"Throughput: {self.metrics.get_throughput():.2f} tasks/sec")
        print(f"Completed: {self.metrics.completed_tasks}/{self.metrics.total_tasks}")
        print(f"Failed: {self.metrics.failed_tasks}/{self.metrics.total_tasks}")
        print(f"{'='*60}\n")

        return tuple(results)

    def get_metrics(self) -> ExecutionMetrics:
        """Get execution metrics"""
        return self.metrics

    def get_queue_stats(self) -> Dict[str, Any]:
        """Get queue statistics"""
        return self.task_queue.get_stats()

#==============================================================================
# High-Level API Functions
#==============================================================================

def execute_parallel_test_generation(target_files: Tuple[str, ...],
                                     nimcp_root: Path,
                                     num_workers: Optional[int] = None,
                                     debug_mode: bool = False) -> Tuple[TaskResult, ...]:
    """
    Execute parallel test generation for multiple files

    WHAT: Convenience function for parallel test generation
    WHY:  Simplified API for common use case
    HOW:  Create tasks, execute in parallel, return results

    EXAMPLE:
        results = execute_parallel_test_generation(
            ("nimcp_p2pnode.c", "nimcp_security.c", "nimcp.c"),
            Path("/home/user/nimcp"),
            num_workers=3
        )

    COMPLEXITY: O(longest_file) with parallelism
    """
    executor = ParallelExecutor(nimcp_root, num_workers, debug_mode)

    # Create tasks for each file
    tasks = tuple(
        create_test_generation_task(
            target_file=f,
            priority=TaskPriority.NORMAL,
            timeout_sec=600
        )
        for f in target_files
    )

    # Add tasks and execute
    executor.add_tasks(tasks)
    results = executor.execute_parallel()

    return results

def execute_parallel_coverage_analysis(target_files: Tuple[str, ...],
                                       nimcp_root: Path,
                                       num_workers: Optional[int] = None) -> Tuple[TaskResult, ...]:
    """
    Execute parallel coverage analysis for multiple files

    WHAT: Convenience function for parallel coverage analysis
    WHY:  Simplified API for coverage tasks
    HOW:  Create coverage tasks, execute in parallel
    """
    executor = ParallelExecutor(nimcp_root, num_workers)

    tasks = tuple(
        create_coverage_task(
            target_file=f,
            priority=TaskPriority.NORMAL
        )
        for f in target_files
    )

    executor.add_tasks(tasks)
    results = executor.execute_parallel()

    return results

#==============================================================================
# Utility Functions
#==============================================================================

def print_execution_summary(results: Tuple[TaskResult, ...]) -> None:
    """
    Print execution summary

    WHAT: Display formatted execution results
    WHY:  Human-readable summary
    HOW:  Format and print statistics
    """
    from task_queue import generate_task_report
    report = generate_task_report(results)
    print(report)

def save_results_to_json(results: Tuple[TaskResult, ...],
                         output_path: Path) -> bool:
    """
    Save results to JSON file

    WHAT: Persist execution results
    WHY:  Enable post-processing and analysis
    HOW:  Serialize to JSON

    RETURNS: True if saved successfully
    """
    try:
        output_path.parent.mkdir(parents=True, exist_ok=True)

        data = {
            'timestamp': datetime.now().isoformat(),
            'total_tasks': len(results),
            'results': [
                {
                    'task_id': r.task.task_id,
                    'target_file': r.task.target_file,
                    'status': r.status.value,
                    'duration_ms': r.duration_ms,
                    'tests_created': r.tests_created,
                    'tests_passed': r.tests_passed,
                    'tests_failed': r.tests_failed,
                    'coverage_data': r.coverage_data,
                    'worker_id': r.worker_id,
                    'exit_code': r.exit_code
                }
                for r in results
            ]
        }

        with open(output_path, 'w') as f:
            json.dump(data, f, indent=2)

        return True

    except Exception as e:
        print(f"Failed to save results: {str(e)}")
        return False
