#!/usr/bin/env python3
"""
Result Aggregator - Aggregate and Merge Parallel Execution Results

WHAT: Aggregates results from parallel Code Surgeon execution
WHY:  Combine distributed results into unified report
HOW:  Merge coverage data, test results, and metrics

PATTERNS: Aggregator, Reducer, Functional
"""

from dataclasses import dataclass, field
from typing import Tuple, Dict, List, Optional, Any
from pathlib import Path
from datetime import datetime
import json

from task_queue import TaskResult, TaskStatus, Task

#==============================================================================
# Aggregated Data Structures
#==============================================================================

@dataclass
class AggregatedCoverage:
    """
    Aggregated coverage data

    WHAT: Combined coverage from all workers
    WHY:  Unified coverage report
    HOW:  Merge coverage data with deduplication
    """
    total_lines: int = 0
    covered_lines: int = 0
    total_branches: int = 0
    covered_branches: int = 0
    total_functions: int = 0
    covered_functions: int = 0

    # Per-file coverage
    file_coverage: Dict[str, Dict[str, Any]] = field(default_factory=dict)

    @property
    def line_coverage_percent(self) -> float:
        """Calculate line coverage percentage"""
        if self.total_lines == 0:
            return 0.0
        return (self.covered_lines / self.total_lines) * 100.0

    @property
    def branch_coverage_percent(self) -> float:
        """Calculate branch coverage percentage"""
        if self.total_branches == 0:
            return 0.0
        return (self.covered_branches / self.total_branches) * 100.0

    @property
    def function_coverage_percent(self) -> float:
        """Calculate function coverage percentage"""
        if self.total_functions == 0:
            return 0.0
        return (self.covered_functions / self.total_functions) * 100.0

@dataclass
class AggregatedMetrics:
    """
    Aggregated execution metrics

    WHAT: Combined metrics from all tasks
    WHY:  Overall execution performance
    HOW:  Aggregate duration, throughput, success rate
    """
    total_tasks: int = 0
    completed_tasks: int = 0
    failed_tasks: int = 0
    timeout_tasks: int = 0

    total_duration_sec: float = 0.0
    min_duration_ms: float = float('inf')
    max_duration_ms: float = 0.0
    avg_duration_ms: float = 0.0

    total_tests_created: int = 0
    total_tests_passed: int = 0
    total_tests_failed: int = 0

    # Per-worker metrics
    worker_metrics: Dict[str, Dict[str, Any]] = field(default_factory=dict)

    @property
    def success_rate(self) -> float:
        """Calculate success rate percentage"""
        if self.total_tasks == 0:
            return 0.0
        return (self.completed_tasks / self.total_tasks) * 100.0

    @property
    def test_pass_rate(self) -> float:
        """Calculate test pass rate"""
        total_tests = self.total_tests_passed + self.total_tests_failed
        if total_tests == 0:
            return 0.0
        return (self.total_tests_passed / total_tests) * 100.0

    @property
    def throughput_tasks_per_sec(self) -> float:
        """Calculate throughput"""
        if self.total_duration_sec == 0:
            return 0.0
        return self.completed_tasks / self.total_duration_sec

@dataclass
class AggregatedReport:
    """
    Complete aggregated report

    WHAT: Unified report combining all aspects
    WHY:  Single source of truth for execution results
    HOW:  Combine coverage, metrics, and raw results
    """
    metrics: AggregatedMetrics
    coverage: AggregatedCoverage
    results: Tuple[TaskResult, ...] = field(default_factory=tuple)
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())

#==============================================================================
# Aggregation Functions (Pure)
#==============================================================================

def aggregate_results(results: Tuple[TaskResult, ...]) -> AggregatedReport:
    """
    Aggregate all task results into unified report

    WHAT: Combine all results into single report
    WHY:  Provide comprehensive view of execution
    HOW:  Merge metrics, coverage, and results

    COMPLEXITY: O(n) where n = number of results
    """
    metrics = aggregate_metrics(results)
    coverage = aggregate_coverage(results)

    return AggregatedReport(
        metrics=metrics,
        coverage=coverage,
        results=results
    )

def aggregate_metrics(results: Tuple[TaskResult, ...]) -> AggregatedMetrics:
    """
    Aggregate execution metrics

    WHAT: Calculate combined metrics from all results
    WHY:  Performance and success analysis
    HOW:  Fold over results accumulating metrics

    COMPLEXITY: O(n)
    """
    if not results:
        return AggregatedMetrics()

    metrics = AggregatedMetrics()
    metrics.total_tasks = len(results)

    durations = []

    for result in results:
        # Count by status
        if result.status == TaskStatus.COMPLETED:
            metrics.completed_tasks += 1
        elif result.status == TaskStatus.FAILED:
            metrics.failed_tasks += 1
        elif result.status == TaskStatus.TIMEOUT:
            metrics.timeout_tasks += 1

        # Duration tracking
        if result.duration_ms > 0:
            durations.append(result.duration_ms)
            metrics.min_duration_ms = min(metrics.min_duration_ms, result.duration_ms)
            metrics.max_duration_ms = max(metrics.max_duration_ms, result.duration_ms)

        # Test counts
        metrics.total_tests_created += result.tests_created
        metrics.total_tests_passed += result.tests_passed
        metrics.total_tests_failed += result.tests_failed

        # Per-worker tracking
        if result.worker_id:
            if result.worker_id not in metrics.worker_metrics:
                metrics.worker_metrics[result.worker_id] = {
                    'tasks_completed': 0,
                    'total_duration_ms': 0.0,
                    'tests_created': 0
                }

            metrics.worker_metrics[result.worker_id]['tasks_completed'] += 1
            metrics.worker_metrics[result.worker_id]['total_duration_ms'] += result.duration_ms
            metrics.worker_metrics[result.worker_id]['tests_created'] += result.tests_created

    # Calculate averages
    if durations:
        metrics.avg_duration_ms = sum(durations) / len(durations)
        metrics.total_duration_sec = sum(durations) / 1000.0

    return metrics

def aggregate_coverage(results: Tuple[TaskResult, ...]) -> AggregatedCoverage:
    """
    Aggregate coverage data

    WHAT: Combine coverage from all tasks
    WHY:  Unified coverage view
    HOW:  Merge coverage data with deduplication

    COMPLEXITY: O(n * f) where n = results, f = files per result
    """
    coverage = AggregatedCoverage()

    for result in results:
        if not result.coverage_data:
            continue

        # Extract coverage data (format depends on tool)
        cov_data = result.coverage_data

        # Aggregate line coverage
        if 'line_coverage' in cov_data:
            coverage.covered_lines += int(cov_data.get('covered_lines', 0))
            coverage.total_lines += int(cov_data.get('total_lines', 0))

        # Aggregate branch coverage
        if 'branch_coverage' in cov_data:
            coverage.covered_branches += int(cov_data.get('covered_branches', 0))
            coverage.total_branches += int(cov_data.get('total_branches', 0))

        # Aggregate function coverage
        if 'function_coverage' in cov_data:
            coverage.covered_functions += int(cov_data.get('covered_functions', 0))
            coverage.total_functions += int(cov_data.get('total_functions', 0))

        # Per-file coverage
        if 'files' in cov_data:
            for file_path, file_cov in cov_data['files'].items():
                coverage.file_coverage[file_path] = file_cov

    return coverage

def merge_coverage_files(coverage_files: Tuple[Path, ...],
                        output_path: Path) -> bool:
    """
    Merge multiple coverage files into one

    WHAT: Combine lcov/gcov files from parallel runs
    WHY:  Unified coverage report from distributed execution
    HOW:  Use lcov --add-tracefile to merge

    NOTE: Side effect - runs lcov and creates file
    RETURNS: True if merge successful
    """
    if not coverage_files:
        return False

    try:
        import subprocess

        # Use lcov to merge files
        cmd = ['lcov']

        for cov_file in coverage_files:
            if cov_file.exists():
                cmd.extend(['--add-tracefile', str(cov_file)])

        cmd.extend(['--output-file', str(output_path)])

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60
        )

        return result.returncode == 0

    except Exception:
        return False

#==============================================================================
# Report Generation
#==============================================================================

def generate_text_report(report: AggregatedReport) -> str:
    """
    Generate human-readable text report

    WHAT: Format aggregated report as text
    WHY:  Console output for users
    HOW:  Format with ASCII box drawing

    COMPLEXITY: O(n) where n = number of results
    """
    m = report.metrics
    c = report.coverage

    text = f"""
╔══════════════════════════════════════════════════════════════
║ PARALLEL CODE SURGEON - AGGREGATED REPORT
║ Generated: {report.timestamp}
╠══════════════════════════════════════════════════════════════
║ EXECUTION METRICS
╠══════════════════════════════════════════════════════════════
║ Total Tasks:          {m.total_tasks}
║ Completed:            {m.completed_tasks} ({m.success_rate:.1f}%)
║ Failed:               {m.failed_tasks}
║ Timeout:              {m.timeout_tasks}
╠══════════════════════════════════════════════════════════════
║ TIMING
╠══════════════════════════════════════════════════════════════
║ Total Duration:       {m.total_duration_sec:.2f}s
║ Average Duration:     {m.avg_duration_ms/1000:.2f}s
║ Min Duration:         {m.min_duration_ms/1000:.2f}s
║ Max Duration:         {m.max_duration_ms/1000:.2f}s
║ Throughput:           {m.throughput_tasks_per_sec:.2f} tasks/sec
╠══════════════════════════════════════════════════════════════
║ TEST RESULTS
╠══════════════════════════════════════════════════════════════
║ Tests Created:        {m.total_tests_created}
║ Tests Passed:         {m.total_tests_passed}
║ Tests Failed:         {m.total_tests_failed}
║ Pass Rate:            {m.test_pass_rate:.1f}%
╠══════════════════════════════════════════════════════════════
║ COVERAGE
╠══════════════════════════════════════════════════════════════
║ Line Coverage:        {c.line_coverage_percent:.1f}% ({c.covered_lines}/{c.total_lines})
║ Branch Coverage:      {c.branch_coverage_percent:.1f}% ({c.covered_branches}/{c.total_branches})
║ Function Coverage:    {c.function_coverage_percent:.1f}% ({c.covered_functions}/{c.total_functions})
╚══════════════════════════════════════════════════════════════
"""

    # Add worker breakdown
    if m.worker_metrics:
        text += "\nWORKER BREAKDOWN:\n"
        for worker_id, worker_data in sorted(m.worker_metrics.items()):
            text += f"\n  {worker_id}:"
            text += f"\n    Tasks: {worker_data['tasks_completed']}"
            text += f"\n    Duration: {worker_data['total_duration_ms']/1000:.2f}s"
            text += f"\n    Tests: {worker_data['tests_created']}"

    # Add per-task summary
    text += "\n\nTASK SUMMARY:\n"
    for i, result in enumerate(report.results, 1):
        status_symbol = "✓" if result.status == TaskStatus.COMPLETED else "✗"
        text += f"\n  [{i}] {status_symbol} {result.task.target_file}"
        text += f"  ({result.duration_ms/1000:.2f}s, {result.tests_created} tests)"

    return text

def generate_json_report(report: AggregatedReport) -> str:
    """
    Generate JSON report

    WHAT: Format aggregated report as JSON
    WHY:  Machine-readable format for tools
    HOW:  Serialize to JSON string

    COMPLEXITY: O(n)
    """
    data = {
        'timestamp': report.timestamp,
        'metrics': {
            'total_tasks': report.metrics.total_tasks,
            'completed_tasks': report.metrics.completed_tasks,
            'failed_tasks': report.metrics.failed_tasks,
            'timeout_tasks': report.metrics.timeout_tasks,
            'success_rate': report.metrics.success_rate,
            'total_duration_sec': report.metrics.total_duration_sec,
            'avg_duration_ms': report.metrics.avg_duration_ms,
            'min_duration_ms': report.metrics.min_duration_ms,
            'max_duration_ms': report.metrics.max_duration_ms,
            'throughput_tasks_per_sec': report.metrics.throughput_tasks_per_sec,
            'total_tests_created': report.metrics.total_tests_created,
            'total_tests_passed': report.metrics.total_tests_passed,
            'total_tests_failed': report.metrics.total_tests_failed,
            'test_pass_rate': report.metrics.test_pass_rate,
            'worker_metrics': report.metrics.worker_metrics
        },
        'coverage': {
            'line_coverage_percent': report.coverage.line_coverage_percent,
            'branch_coverage_percent': report.coverage.branch_coverage_percent,
            'function_coverage_percent': report.coverage.function_coverage_percent,
            'total_lines': report.coverage.total_lines,
            'covered_lines': report.coverage.covered_lines,
            'total_branches': report.coverage.total_branches,
            'covered_branches': report.coverage.covered_branches,
            'file_coverage': report.coverage.file_coverage
        },
        'results': [
            {
                'task_id': r.task.task_id,
                'target_file': r.task.target_file,
                'status': r.status.value,
                'duration_ms': r.duration_ms,
                'tests_created': r.tests_created,
                'tests_passed': r.tests_passed,
                'tests_failed': r.tests_failed,
                'worker_id': r.worker_id,
                'exit_code': r.exit_code
            }
            for r in report.results
        ]
    }

    return json.dumps(data, indent=2)

def generate_html_report(report: AggregatedReport,
                        output_path: Path) -> bool:
    """
    Generate HTML report

    WHAT: Create interactive HTML report
    WHY:  Rich visualization of results
    HOW:  Generate HTML with CSS/JS

    NOTE: Side effect - creates HTML file
    RETURNS: True if successful
    """
    try:
        m = report.metrics
        c = report.coverage

        html = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Code Surgeon - Parallel Execution Report</title>
    <meta charset="UTF-8">
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            margin: 0;
            padding: 20px;
            background: #f5f5f5;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }}
        h1 {{
            color: #333;
            border-bottom: 3px solid #4CAF50;
            padding-bottom: 10px;
        }}
        .metrics {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin: 30px 0;
        }}
        .metric-card {{
            background: #f9f9f9;
            padding: 20px;
            border-radius: 6px;
            border-left: 4px solid #4CAF50;
        }}
        .metric-value {{
            font-size: 2em;
            font-weight: bold;
            color: #4CAF50;
        }}
        .metric-label {{
            color: #666;
            margin-top: 5px;
        }}
        .progress-bar {{
            width: 100%;
            height: 20px;
            background: #e0e0e0;
            border-radius: 10px;
            overflow: hidden;
            margin: 10px 0;
        }}
        .progress-fill {{
            height: 100%;
            background: linear-gradient(90deg, #4CAF50, #8BC34A);
            transition: width 0.3s;
        }}
        table {{
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }}
        th, td {{
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }}
        th {{
            background: #4CAF50;
            color: white;
        }}
        tr:hover {{
            background: #f5f5f5;
        }}
        .status-completed {{
            color: #4CAF50;
            font-weight: bold;
        }}
        .status-failed {{
            color: #f44336;
            font-weight: bold;
        }}
        .timestamp {{
            color: #999;
            font-size: 0.9em;
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>Code Surgeon - Parallel Execution Report</h1>
        <p class="timestamp">Generated: {report.timestamp}</p>

        <div class="metrics">
            <div class="metric-card">
                <div class="metric-value">{m.total_tasks}</div>
                <div class="metric-label">Total Tasks</div>
            </div>
            <div class="metric-card">
                <div class="metric-value">{m.success_rate:.1f}%</div>
                <div class="metric-label">Success Rate</div>
                <div class="progress-bar">
                    <div class="progress-fill" style="width: {m.success_rate}%"></div>
                </div>
            </div>
            <div class="metric-card">
                <div class="metric-value">{m.total_duration_sec:.1f}s</div>
                <div class="metric-label">Total Duration</div>
            </div>
            <div class="metric-card">
                <div class="metric-value">{m.total_tests_created}</div>
                <div class="metric-label">Tests Created</div>
            </div>
        </div>

        <h2>Coverage</h2>
        <div class="metrics">
            <div class="metric-card">
                <div class="metric-value">{c.line_coverage_percent:.1f}%</div>
                <div class="metric-label">Line Coverage</div>
                <div class="progress-bar">
                    <div class="progress-fill" style="width: {c.line_coverage_percent}%"></div>
                </div>
            </div>
            <div class="metric-card">
                <div class="metric-value">{c.branch_coverage_percent:.1f}%</div>
                <div class="metric-label">Branch Coverage</div>
                <div class="progress-bar">
                    <div class="progress-fill" style="width: {c.branch_coverage_percent}%"></div>
                </div>
            </div>
        </div>

        <h2>Task Results</h2>
        <table>
            <thead>
                <tr>
                    <th>Task</th>
                    <th>Status</th>
                    <th>Duration</th>
                    <th>Tests Created</th>
                    <th>Tests Passed</th>
                    <th>Worker</th>
                </tr>
            </thead>
            <tbody>
"""

        for result in report.results:
            status_class = "status-completed" if result.status == TaskStatus.COMPLETED else "status-failed"
            html += f"""
                <tr>
                    <td>{result.task.target_file}</td>
                    <td class="{status_class}">{result.status.value}</td>
                    <td>{result.duration_ms/1000:.2f}s</td>
                    <td>{result.tests_created}</td>
                    <td>{result.tests_passed}</td>
                    <td>{result.worker_id or 'N/A'}</td>
                </tr>
"""

        html += """
            </tbody>
        </table>
    </div>
</body>
</html>
"""

        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(html)

        return True

    except Exception:
        return False

#==============================================================================
# Utility Functions
#==============================================================================

def save_report(report: AggregatedReport,
                output_dir: Path,
                formats: Tuple[str, ...] = ('text', 'json', 'html')) -> Dict[str, Path]:
    """
    Save report in multiple formats

    WHAT: Export report to disk in various formats
    WHY:  Support different consumption methods
    HOW:  Generate and write each format

    RETURNS: Dict mapping format to output path
    COMPLEXITY: O(n * f) where n = results, f = formats
    """
    output_dir.mkdir(parents=True, exist_ok=True)
    outputs = {}

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

    if 'text' in formats:
        text_path = output_dir / f"report_{timestamp}.txt"
        text_path.write_text(generate_text_report(report))
        outputs['text'] = text_path

    if 'json' in formats:
        json_path = output_dir / f"report_{timestamp}.json"
        json_path.write_text(generate_json_report(report))
        outputs['json'] = json_path

    if 'html' in formats:
        html_path = output_dir / f"report_{timestamp}.html"
        generate_html_report(report, html_path)
        outputs['html'] = html_path

    return outputs
