#!/usr/bin/env python3
"""
Task Queue - Distributed Task Management for Code Surgeon

WHAT: Thread-safe task queue for parallel test execution
WHY:  Distribute work across multiple Code Surgeon agents
HOW:  Queue-based work distribution with priority support

PATTERNS: Producer-Consumer, Thread-Safe Queue, Priority Queue
"""

from dataclasses import dataclass, field
from typing import Tuple, Optional, List, Dict, Any
from enum import Enum
from pathlib import Path
import queue
import threading
from datetime import datetime

#==============================================================================
# Data Structures
#==============================================================================

class TaskPriority(Enum):
    """Task priority levels"""
    CRITICAL = 0  # Must run first (hanging tests that block others)
    HIGH = 1      # Important tests (core functionality)
    NORMAL = 2    # Standard tests
    LOW = 3       # Optional tests (documentation, etc)

class TaskStatus(Enum):
    """Task execution status"""
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    TIMEOUT = "timeout"
    CANCELLED = "cancelled"

@dataclass(frozen=True)
class Task:
    """
    Immutable task representation

    WHAT: Single unit of work for Code Surgeon agent
    WHY:  Encapsulate all info needed for independent execution
    HOW:  Frozen dataclass with task metadata
    """
    task_id: str
    target_file: str  # e.g., "nimcp_p2pnode.c"
    test_type: str    # "unit", "integration", etc
    priority: TaskPriority = TaskPriority.NORMAL
    timeout_sec: int = 300
    retry_count: int = 0
    max_retries: int = 2
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: str = field(default_factory=lambda: datetime.now().isoformat())

    def __lt__(self, other):
        """Enable priority queue sorting"""
        return self.priority.value < other.priority.value

@dataclass
class TaskResult:
    """
    Mutable task result (modified by child agents)

    WHAT: Result of task execution
    WHY:  Capture output, status, timing for aggregation
    HOW:  Dataclass with execution details
    """
    task: Task
    status: TaskStatus
    duration_ms: float
    stdout: str = ""
    stderr: str = ""
    exit_code: int = 0
    coverage_data: Optional[Dict[str, Any]] = None
    tests_created: int = 0
    tests_passed: int = 0
    tests_failed: int = 0
    completed_at: str = field(default_factory=lambda: datetime.now().isoformat())
    worker_id: Optional[str] = None

#==============================================================================
# Task Queue (Thread-Safe)
#==============================================================================

class TaskQueue:
    """
    Thread-safe priority task queue

    WHAT: Manages task distribution to workers
    WHY:  Enable parallel execution with priority
    HOW:  PriorityQueue with thread safety

    THREAD SAFETY: All operations are thread-safe
    """

    def __init__(self, max_size: int = 1000):
        """
        Initialize task queue

        WHAT: Create priority queue with size limit
        WHY:  Prevent unbounded memory growth
        HOW:  PriorityQueue wrapper
        """
        self._queue = queue.PriorityQueue(maxsize=max_size)
        self._lock = threading.Lock()
        self._task_registry: Dict[str, Task] = {}
        self._completed: Dict[str, TaskResult] = {}
        self._stats = {
            'total_added': 0,
            'total_completed': 0,
            'total_failed': 0,
            'total_timeout': 0
        }

    def add_task(self, task: Task) -> bool:
        """
        Add task to queue (thread-safe)

        WHAT: Enqueue task for execution
        WHY:  Producer adds work for consumers
        HOW:  PriorityQueue.put with thread safety

        RETURNS: True if added, False if queue full
        COMPLEXITY: O(log n) for priority queue insertion
        """
        try:
            # Priority queue uses (priority, task) tuples
            self._queue.put((task.priority, task), timeout=1.0)

            with self._lock:
                self._task_registry[task.task_id] = task
                self._stats['total_added'] += 1

            return True

        except queue.Full:
            return False

    def add_tasks(self, tasks: Tuple[Task, ...]) -> int:
        """
        Add multiple tasks (thread-safe)

        WHAT: Bulk task addition
        WHY:  More efficient than individual adds
        HOW:  Loop with add_task

        RETURNS: Number of tasks successfully added
        COMPLEXITY: O(n log m) where n = tasks, m = queue size
        """
        added = 0
        for task in tasks:
            if self.add_task(task):
                added += 1
        return added

    def get_task(self, timeout_sec: Optional[float] = None) -> Optional[Task]:
        """
        Get next task (thread-safe, blocking)

        WHAT: Consumer retrieves task from queue
        WHY:  Worker pulls next work item
        HOW:  PriorityQueue.get with timeout

        RETURNS: Task or None if timeout/empty
        COMPLEXITY: O(log n) for priority queue extraction
        """
        try:
            priority, task = self._queue.get(timeout=timeout_sec)
            return task

        except queue.Empty:
            return None

    def complete_task(self, result: TaskResult) -> None:
        """
        Mark task as completed (thread-safe)

        WHAT: Record task result
        WHY:  Track completion for aggregation
        HOW:  Update registry and stats

        NOTE: Side effect - updates internal state
        """
        with self._lock:
            self._completed[result.task.task_id] = result

            if result.status == TaskStatus.COMPLETED:
                self._stats['total_completed'] += 1
            elif result.status == TaskStatus.FAILED:
                self._stats['total_failed'] += 1
            elif result.status == TaskStatus.TIMEOUT:
                self._stats['total_timeout'] += 1

    def retry_task(self, task: Task) -> bool:
        """
        Retry failed task (thread-safe)

        WHAT: Re-enqueue failed task with incremented retry count
        WHY:  Handle transient failures
        HOW:  Create new task with retry_count + 1

        RETURNS: True if retried, False if max retries exceeded
        """
        if task.retry_count >= task.max_retries:
            return False

        # Create new task with incremented retry
        retry_task = Task(
            task_id=f"{task.task_id}_retry{task.retry_count + 1}",
            target_file=task.target_file,
            test_type=task.test_type,
            priority=task.priority,
            timeout_sec=task.timeout_sec,
            retry_count=task.retry_count + 1,
            max_retries=task.max_retries,
            metadata={**task.metadata, 'original_task_id': task.task_id}
        )

        return self.add_task(retry_task)

    def get_stats(self) -> Dict[str, Any]:
        """
        Get queue statistics (thread-safe)

        WHAT: Retrieve current queue state
        WHY:  Monitor progress
        HOW:  Read-only access to stats

        RETURNS: Dict with queue statistics
        COMPLEXITY: O(1)
        """
        with self._lock:
            return {
                **self._stats,
                'queue_size': self._queue.qsize(),
                'pending': self._stats['total_added'] - self._stats['total_completed'] -
                          self._stats['total_failed'] - self._stats['total_timeout'],
                'completion_rate': (self._stats['total_completed'] / self._stats['total_added'] * 100)
                                  if self._stats['total_added'] > 0 else 0.0
            }

    def get_completed_results(self) -> Tuple[TaskResult, ...]:
        """
        Get all completed task results (thread-safe)

        WHAT: Retrieve results for aggregation
        WHY:  Parent needs all results for reporting
        HOW:  Return tuple of all completed

        RETURNS: Tuple of TaskResult objects
        COMPLEXITY: O(n) where n = completed tasks
        """
        with self._lock:
            return tuple(self._completed.values())

    def is_empty(self) -> bool:
        """Check if queue is empty (thread-safe)"""
        return self._queue.empty()

    def size(self) -> int:
        """Get current queue size (thread-safe)"""
        return self._queue.qsize()

    def clear(self) -> None:
        """Clear all pending tasks (thread-safe)"""
        with self._lock:
            while not self._queue.empty():
                try:
                    self._queue.get_nowait()
                except queue.Empty:
                    break

#==============================================================================
# Task Factory Functions (Pure)
#==============================================================================

def create_test_generation_task(target_file: str,
                                  priority: TaskPriority = TaskPriority.NORMAL,
                                  timeout_sec: int = 300) -> Task:
    """
    Create task for test generation

    WHAT: Factory function for test generation tasks
    WHY:  Convenience function for common task type
    HOW:  Create Task with test generation metadata

    COMPLEXITY: O(1)
    """
    task_id = f"test_gen_{Path(target_file).stem}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

    return Task(
        task_id=task_id,
        target_file=target_file,
        test_type="unit",
        priority=priority,
        timeout_sec=timeout_sec,
        metadata={
            'operation': 'generate_tests',
            'mode': 'full'
        }
    )

def create_coverage_task(target_file: str,
                         priority: TaskPriority = TaskPriority.NORMAL) -> Task:
    """
    Create task for coverage analysis

    WHAT: Factory function for coverage tasks
    WHY:  Convenience function for coverage analysis
    HOW:  Create Task with coverage metadata
    """
    task_id = f"coverage_{Path(target_file).stem}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

    return Task(
        task_id=task_id,
        target_file=target_file,
        test_type="coverage",
        priority=priority,
        timeout_sec=600,  # Coverage takes longer
        metadata={
            'operation': 'analyze_coverage',
            'generate_report': True
        }
    )

def create_fix_task(test_name: str,
                    failure_type: str,
                    priority: TaskPriority = TaskPriority.HIGH) -> Task:
    """
    Create task for test fixing

    WHAT: Factory function for fix tasks
    WHY:  Convenience function for fixing failures
    HOW:  Create Task with fix metadata
    """
    task_id = f"fix_{test_name}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

    return Task(
        task_id=task_id,
        target_file=test_name,
        test_type="fix",
        priority=priority,
        timeout_sec=300,
        metadata={
            'operation': 'fix_test',
            'failure_type': failure_type
        }
    )

#==============================================================================
# Utility Functions
#==============================================================================

def generate_task_report(results: Tuple[TaskResult, ...]) -> str:
    """
    Generate human-readable task report

    WHAT: Format task results for display
    WHY:  Summarize parallel execution results
    HOW:  Aggregate and format statistics

    COMPLEXITY: O(n) where n = results
    """
    if not results:
        return "No tasks completed"

    total = len(results)
    completed = sum(1 for r in results if r.status == TaskStatus.COMPLETED)
    failed = sum(1 for r in results if r.status == TaskStatus.FAILED)
    timeout = sum(1 for r in results if r.status == TaskStatus.TIMEOUT)

    total_duration = sum(r.duration_ms for r in results) / 1000.0  # Convert to seconds
    avg_duration = total_duration / total if total > 0 else 0.0

    total_tests_created = sum(r.tests_created for r in results)
    total_tests_passed = sum(r.tests_passed for r in results)
    total_tests_failed = sum(r.tests_failed for r in results)

    report = f"""
╔══════════════════════════════════════════════════════════════
║ PARALLEL EXECUTION REPORT
╠══════════════════════════════════════════════════════════════
║ Total Tasks:          {total}
║ Completed:            {completed} ({completed/total*100:.1f}%)
║ Failed:               {failed} ({failed/total*100:.1f}%)
║ Timeout:              {timeout} ({timeout/total*100:.1f}%)
╠══════════════════════════════════════════════════════════════
║ Total Duration:       {total_duration:.2f}s
║ Average Duration:     {avg_duration:.2f}s
║ Tests Created:        {total_tests_created}
║ Tests Passed:         {total_tests_passed}
║ Tests Failed:         {total_tests_failed}
╚══════════════════════════════════════════════════════════════

DETAILED RESULTS:
"""

    for i, result in enumerate(results, 1):
        status_symbol = "✓" if result.status == TaskStatus.COMPLETED else "✗"
        report += f"\n  [{i}] {status_symbol} {result.task.target_file}"
        report += f"\n      Status: {result.status.value}"
        report += f"\n      Duration: {result.duration_ms/1000:.2f}s"
        report += f"\n      Tests: {result.tests_created} created, {result.tests_passed} passed"
        if result.worker_id:
            report += f"\n      Worker: {result.worker_id}"

    return report
