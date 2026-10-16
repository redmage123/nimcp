#!/usr/bin/env python3
"""
NIMCP Debugging Suite - Code Surgeon
====================================

A comprehensive debugging toolkit that systematically analyzes test failures,
memory issues, deadlocks, and crashes using multiple tools in sequence.

Tools integrated:
- valgrind (memcheck, helgrind, drd)
- gdb (breakpoints, backtraces, watchpoints)
- rr (record & replay debugging)
- libFuzzer (proactive bug finding)
- ctags (code navigation)
- Address Sanitizer (ASan)
- Thread Sanitizer (TSan)
- Undefined Behavior Sanitizer (UBSan)

Usage:
    ./debug_suite.py --test <test_binary> [--filter <gtest_filter>] [--mode <mode>]

Modes:
    auto        - Automatically select best tool based on symptoms (default)
    quick       - Fast check with basic tools
    memory      - Focus on memory issues (valgrind, ASan)
    threading   - Focus on threading issues (helgrind, TSan)
    crash       - Debug crashes (gdb with core dumps)
    record      - Record with rr for replay debugging
    fuzz        - Run libFuzzer to proactively find bugs
    interactive - Drop into interactive gdb session

Fuzzing Example:
    ./debug_suite.py --test dummy --mode fuzz --fuzz-target ./src/fuzz/fuzz_btree --fuzz-duration 600
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import shutil


@dataclass
class DebugResult:
    """Results from a debugging tool run"""
    tool: str
    success: bool
    exit_code: int
    stdout: str
    stderr: str
    duration: float
    issues_found: List[Dict] = field(default_factory=list)
    recommendations: List[str] = field(default_factory=list)

    def to_dict(self):
        return {
            'tool': self.tool,
            'success': self.success,
            'exit_code': self.exit_code,
            'duration': self.duration,
            'issues_found': self.issues_found,
            'recommendations': self.recommendations,
        }


@dataclass
class DebugSession:
    """A complete debugging session"""
    test_binary: str
    test_filter: Optional[str]
    mode: str
    timestamp: str
    results: List[DebugResult] = field(default_factory=list)

    def add_result(self, result: DebugResult):
        self.results.append(result)

    def save(self, report_dir: Path):
        """Save session to JSON file"""
        report_file = report_dir / f"debug_{self.timestamp}.json"
        with open(report_file, 'w') as f:
            json.dump({
                'test_binary': self.test_binary,
                'test_filter': self.test_filter,
                'mode': self.mode,
                'timestamp': self.timestamp,
                'results': [r.to_dict() for r in self.results]
            }, f, indent=2)
        return report_file


class DebugTool:
    """Base class for debugging tools"""

    def __init__(self, session: DebugSession):
        self.session = session

    def is_available(self) -> bool:
        """Check if tool is installed"""
        raise NotImplementedError

    def run(self, test_cmd: List[str], **kwargs) -> DebugResult:
        """Run the tool"""
        raise NotImplementedError

    def parse_output(self, stdout: str, stderr: str) -> Tuple[List[Dict], List[str]]:
        """Parse tool output into issues and recommendations"""
        return [], []


class ValgrindTool(DebugTool):
    """Valgrind memory checker"""

    def is_available(self) -> bool:
        return shutil.which('valgrind') is not None

    def run(self, test_cmd: List[str], tool: str = 'memcheck', **kwargs) -> DebugResult:
        start = time.time()

        valgrind_cmd = [
            'valgrind',
            f'--tool={tool}',
            '--leak-check=full',
            '--show-leak-kinds=all',
            '--track-origins=yes',
            '--verbose',
            '--error-exitcode=1',
        ]

        if tool == 'helgrind':
            valgrind_cmd.extend(['--history-level=approx'])
        elif tool == 'drd':
            valgrind_cmd.extend(['--check-stack-var=yes'])

        full_cmd = valgrind_cmd + test_cmd

        print(f"Running: {' '.join(full_cmd)}")

        try:
            result = subprocess.run(
                full_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=300,
                text=True
            )

            issues, recommendations = self.parse_output(result.stdout, result.stderr)

            return DebugResult(
                tool=f'valgrind_{tool}',
                success=(result.returncode == 0),
                exit_code=result.returncode,
                stdout=result.stdout,
                stderr=result.stderr,
                duration=time.time() - start,
                issues_found=issues,
                recommendations=recommendations
            )
        except subprocess.TimeoutExpired:
            return DebugResult(
                tool=f'valgrind_{tool}',
                success=False,
                exit_code=-1,
                stdout='',
                stderr='Timeout after 300s',
                duration=time.time() - start,
                issues_found=[{'type': 'timeout', 'message': 'Test did not complete in 300s'}],
                recommendations=['Test may be deadlocked', 'Try helgrind for threading issues']
            )

    def parse_output(self, stdout: str, stderr: str) -> Tuple[List[Dict], List[str]]:
        issues = []
        recommendations = []

        # Parse memory leaks
        leak_pattern = r'==\d+== (\d+) bytes in (\d+) blocks are definitely lost'
        for match in re.finditer(leak_pattern, stderr):
            issues.append({
                'type': 'memory_leak',
                'bytes': match.group(1),
                'blocks': match.group(2)
            })
            recommendations.append(f"Fix memory leak: {match.group(1)} bytes in {match.group(2)} blocks")

        # Parse invalid reads/writes
        invalid_pattern = r'==\d+== Invalid (read|write) of size (\d+)'
        for match in re.finditer(invalid_pattern, stderr):
            issues.append({
                'type': f'invalid_{match.group(1)}',
                'size': match.group(2)
            })
            recommendations.append(f"Fix invalid {match.group(1)} of size {match.group(2)}")

        # Parse use after free
        if 'Invalid free()' in stderr or 'double free' in stderr:
            issues.append({'type': 'double_free'})
            recommendations.append('Fix double free - likely freeing memory twice')

        # Parse data races (helgrind)
        if 'Possible data race' in stderr:
            issues.append({'type': 'data_race'})
            recommendations.append('Fix data race - missing synchronization')

        # Parse lock order violations
        if 'lock order violation' in stderr:
            issues.append({'type': 'lock_order_violation'})
            recommendations.append('Fix lock order violation - potential deadlock')

        return issues, recommendations


class GDBTool(DebugTool):
    """GDB debugger"""

    def is_available(self) -> bool:
        return shutil.which('gdb') is not None

    def run(self, test_cmd: List[str], breakpoints: List[str] = None, **kwargs) -> DebugResult:
        start = time.time()

        # Create GDB command file
        gdb_commands = [
            'set pagination off',
            'set print pretty on',
            'run',
        ]

        if breakpoints:
            gdb_commands = ['set pagination off'] + [f'break {bp}' for bp in breakpoints] + ['run']

        # Add backtrace on crash
        gdb_commands.extend([
            'backtrace full',
            'info threads',
            'thread apply all backtrace',
            'quit'
        ])

        gdb_script = Path('/tmp/gdb_commands.txt')
        gdb_script.write_text('\n'.join(gdb_commands))

        gdb_cmd = ['gdb', '--batch', '-x', str(gdb_script), '--args'] + test_cmd

        print(f"Running GDB with commands: {gdb_commands}")

        try:
            result = subprocess.run(
                gdb_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=300,
                text=True
            )

            issues, recommendations = self.parse_output(result.stdout, result.stderr)

            return DebugResult(
                tool='gdb',
                success=(result.returncode == 0),
                exit_code=result.returncode,
                stdout=result.stdout,
                stderr=result.stderr,
                duration=time.time() - start,
                issues_found=issues,
                recommendations=recommendations
            )
        except subprocess.TimeoutExpired:
            return DebugResult(
                tool='gdb',
                success=False,
                exit_code=-1,
                stdout='',
                stderr='Timeout after 300s',
                duration=time.time() - start,
                issues_found=[{'type': 'timeout', 'message': 'Test hung in GDB'}],
                recommendations=['Test is likely deadlocked', 'Check thread states with helgrind']
            )

    def parse_output(self, stdout: str, stderr: str) -> Tuple[List[Dict], List[str]]:
        issues = []
        recommendations = []

        # Parse segfaults
        if 'SIGSEGV' in stdout or 'Segmentation fault' in stdout:
            issues.append({'type': 'segfault'})
            recommendations.append('Segmentation fault detected - check backtrace for null pointer or bad memory access')

        # Parse aborts
        if 'SIGABRT' in stdout or 'Aborted' in stdout:
            issues.append({'type': 'abort'})
            recommendations.append('Abort signal - likely assertion failure or double free')

        # Extract backtrace
        bt_pattern = r'#\d+\s+0x[0-9a-f]+\s+in\s+(\S+)'
        backtraces = re.findall(bt_pattern, stdout)
        if backtraces:
            issues.append({'type': 'backtrace', 'functions': backtraces[:10]})

        # Parse deadlock indicators
        if 'pthread_rwlock_wrlock' in stdout or 'pthread_mutex_lock' in stdout:
            lock_count = stdout.count('pthread_rwlock_wrlock') + stdout.count('pthread_mutex_lock')
            if lock_count > 1:
                issues.append({'type': 'potential_deadlock', 'lock_count': lock_count})
                recommendations.append(f'Multiple threads waiting on locks ({lock_count}) - potential deadlock')

        return issues, recommendations


class RRTool(DebugTool):
    """rr record and replay debugger"""

    def is_available(self) -> bool:
        return shutil.which('rr') is not None

    def run(self, test_cmd: List[str], **kwargs) -> DebugResult:
        start = time.time()

        # First, record the execution
        record_cmd = ['rr', 'record'] + test_cmd

        print(f"Recording with rr: {' '.join(record_cmd)}")

        try:
            result = subprocess.run(
                record_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=300,
                text=True
            )

            # Get the latest trace
            trace_result = subprocess.run(
                ['rr', 'ls'],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )

            traces = [line.strip() for line in trace_result.stdout.split('\n') if line.strip()]
            latest_trace = traces[-1] if traces else None

            recommendations = []
            if latest_trace:
                recommendations.append(f'Recording saved. Replay with: rr replay {latest_trace}')
                recommendations.append(f'Use "reverse-next" and "reverse-step" to debug backwards')

            return DebugResult(
                tool='rr',
                success=(result.returncode == 0),
                exit_code=result.returncode,
                stdout=result.stdout,
                stderr=result.stderr,
                duration=time.time() - start,
                issues_found=[],
                recommendations=recommendations
            )
        except subprocess.TimeoutExpired:
            return DebugResult(
                tool='rr',
                success=False,
                exit_code=-1,
                stdout='',
                stderr='Timeout after 300s',
                duration=time.time() - start,
                issues_found=[{'type': 'timeout'}],
                recommendations=['Test timed out during recording']
            )


class SanitizerTool(DebugTool):
    """Address/Thread/Undefined Behavior Sanitizers"""

    def __init__(self, session: DebugSession, sanitizer: str = 'address'):
        super().__init__(session)
        self.sanitizer = sanitizer

    def is_available(self) -> bool:
        # Check if test binary was built with sanitizer
        # For now, just check if we can run it
        return True

    def run(self, test_cmd: List[str], **kwargs) -> DebugResult:
        start = time.time()

        # Set environment variables for sanitizer
        env = os.environ.copy()

        if self.sanitizer == 'address':
            env['ASAN_OPTIONS'] = 'detect_leaks=1:check_initialization_order=1:strict_init_order=1'
        elif self.sanitizer == 'thread':
            env['TSAN_OPTIONS'] = 'history_size=7:second_deadlock_stack=1'
        elif self.sanitizer == 'undefined':
            env['UBSAN_OPTIONS'] = 'print_stacktrace=1'

        print(f"Running with {self.sanitizer} sanitizer")

        try:
            result = subprocess.run(
                test_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=300,
                text=True,
                env=env
            )

            issues, recommendations = self.parse_output(result.stdout, result.stderr)

            return DebugResult(
                tool=f'{self.sanitizer}_sanitizer',
                success=(result.returncode == 0),
                exit_code=result.returncode,
                stdout=result.stdout,
                stderr=result.stderr,
                duration=time.time() - start,
                issues_found=issues,
                recommendations=recommendations
            )
        except subprocess.TimeoutExpired:
            return DebugResult(
                tool=f'{self.sanitizer}_sanitizer',
                success=False,
                exit_code=-1,
                stdout='',
                stderr='Timeout after 300s',
                duration=time.time() - start,
                issues_found=[{'type': 'timeout'}],
                recommendations=['Test timed out']
            )

    def parse_output(self, stdout: str, stderr: str) -> Tuple[List[Dict], List[str]]:
        issues = []
        recommendations = []
        combined = stdout + stderr

        # ASan patterns
        if 'heap-use-after-free' in combined:
            issues.append({'type': 'use_after_free'})
            recommendations.append('Use-after-free detected - accessing freed memory')

        if 'heap-buffer-overflow' in combined:
            issues.append({'type': 'buffer_overflow'})
            recommendations.append('Heap buffer overflow - writing beyond allocated memory')

        if 'double-free' in combined:
            issues.append({'type': 'double_free'})
            recommendations.append('Double-free detected - freeing memory twice')

        # TSan patterns
        if 'data race' in combined:
            issues.append({'type': 'data_race'})
            recommendations.append('Data race detected - unsynchronized access to shared memory')

        if 'lock-order-inversion' in combined:
            issues.append({'type': 'lock_order_inversion'})
            recommendations.append('Lock order inversion - potential deadlock')

        # UBSan patterns
        if 'null pointer' in combined:
            issues.append({'type': 'null_pointer_deref'})
            recommendations.append('Null pointer dereference')

        if 'signed integer overflow' in combined:
            issues.append({'type': 'integer_overflow'})
            recommendations.append('Signed integer overflow - undefined behavior')

        return issues, recommendations


class FuzzTool(DebugTool):
    """LibFuzzer-based fuzzing tool"""

    def __init__(self, session: DebugSession, fuzz_target: str = None):
        super().__init__(session)
        self.fuzz_target = fuzz_target

    def is_available(self) -> bool:
        # Check if fuzzer binary exists
        if self.fuzz_target:
            fuzz_binary = Path(self.fuzz_target)
            return fuzz_binary.exists()
        return False

    def run(self, test_cmd: List[str] = None, duration: int = None, **kwargs) -> DebugResult:
        start = time.time()

        # Use provided duration, or check session for fuzz_duration, or default to 300
        if duration is None:
            duration = getattr(self.session, 'fuzz_duration', 300)

        if not self.fuzz_target:
            return DebugResult(
                tool='fuzzer',
                success=False,
                exit_code=-1,
                stdout='',
                stderr='No fuzz target specified',
                duration=0,
                issues_found=[],
                recommendations=['Specify fuzz target with --fuzz-target']
            )

        # Create corpus directory
        corpus_dir = Path(f'corpus_{Path(self.fuzz_target).stem}')
        corpus_dir.mkdir(exist_ok=True)

        # Build fuzzer command
        fuzz_cmd = [
            self.fuzz_target,
            str(corpus_dir),
            f'-max_total_time={duration}',
            '-print_final_stats=1',
            '-detect_leaks=1'
        ]

        print(f"Running fuzzer: {' '.join(fuzz_cmd)}")
        print(f"Duration: {duration}s, Corpus: {corpus_dir}")

        try:
            result = subprocess.run(
                fuzz_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=duration + 30,  # Extra time for cleanup
                text=True
            )

            issues, recommendations = self.parse_output(result.stdout, result.stderr)

            # Check for crash files
            crash_files = list(Path('.').glob('crash-*')) + list(Path('.').glob('leak-*'))
            if crash_files:
                issues.append({
                    'type': 'fuzzer_crashes',
                    'count': len(crash_files),
                    'files': [str(f) for f in crash_files[:5]]
                })
                recommendations.append(f'Found {len(crash_files)} crash files')
                for crash_file in crash_files[:3]:
                    recommendations.append(f'Reproduce with: {self.fuzz_target} {crash_file}')

            return DebugResult(
                tool='fuzzer',
                success=(result.returncode == 0 and len(crash_files) == 0),
                exit_code=result.returncode,
                stdout=result.stdout,
                stderr=result.stderr,
                duration=time.time() - start,
                issues_found=issues,
                recommendations=recommendations
            )

        except subprocess.TimeoutExpired:
            return DebugResult(
                tool='fuzzer',
                success=False,
                exit_code=-1,
                stdout='',
                stderr=f'Fuzzer did not complete within {duration + 30}s',
                duration=time.time() - start,
                issues_found=[{'type': 'timeout'}],
                recommendations=['Fuzzer timed out - may need longer duration']
            )

    def parse_output(self, stdout: str, stderr: str) -> Tuple[List[Dict], List[str]]:
        issues = []
        recommendations = []
        combined = stdout + stderr

        # Parse fuzzer statistics
        cov_match = re.search(r'cov:\s*(\d+)', combined)
        if cov_match:
            coverage = int(cov_match.group(1))
            issues.append({'type': 'coverage', 'edges': coverage})
            recommendations.append(f'Coverage: {coverage} edges')

        exec_match = re.search(r'exec/s:\s*(\d+)', combined)
        if exec_match:
            exec_per_sec = int(exec_match.group(1))
            recommendations.append(f'Speed: {exec_per_sec} executions/second')

        # Parse crashes
        if 'ERROR: AddressSanitizer' in combined:
            issues.append({'type': 'asan_error'})

            # Extract specific error type
            if 'heap-buffer-overflow' in combined:
                issues.append({'type': 'heap_buffer_overflow'})
                recommendations.append('Heap buffer overflow detected by fuzzer')
            elif 'heap-use-after-free' in combined:
                issues.append({'type': 'use_after_free'})
                recommendations.append('Use-after-free detected by fuzzer')
            elif 'double-free' in combined:
                issues.append({'type': 'double_free'})
                recommendations.append('Double-free detected by fuzzer')

        if 'CONSISTENCY ERROR' in combined:
            issues.append({'type': 'consistency_error'})
            recommendations.append('B-tree consistency check failed - count mismatch')

        if 'SUMMARY:' in combined and 'leak' in combined:
            issues.append({'type': 'memory_leak'})
            recommendations.append('Memory leak detected by fuzzer')

        # Parse corpus growth
        corp_match = re.search(r'corp:\s*(\d+)/(\d+)b', combined)
        if corp_match:
            corpus_size = int(corp_match.group(1))
            corpus_bytes = int(corp_match.group(2))
            recommendations.append(f'Corpus: {corpus_size} files, {corpus_bytes} bytes')

        return issues, recommendations


class DebugSuite:
    """Main debugging suite orchestrator"""

    def __init__(self, test_binary: str, test_filter: Optional[str] = None, mode: str = 'auto'):
        self.test_binary = test_binary
        self.test_filter = test_filter
        self.mode = mode
        self.session = DebugSession(
            test_binary=test_binary,
            test_filter=test_filter,
            mode=mode,
            timestamp=datetime.now().strftime('%Y%m%d_%H%M%S')
        )
        self.report_dir = Path('/home/bbrelin/nimcp/debug_reports')
        self.report_dir.mkdir(exist_ok=True)

    def build_test_command(self) -> List[str]:
        """Build the test command"""
        cmd = [self.test_binary]
        if self.test_filter:
            cmd.extend(['--gtest_filter=' + self.test_filter])
        return cmd

    def detect_symptoms(self) -> List[str]:
        """Run test once to detect symptoms"""
        print("\n=== Detecting symptoms ===")
        test_cmd = self.build_test_command()

        try:
            result = subprocess.run(
                test_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=60,
                text=True
            )

            symptoms = []
            combined = result.stdout + result.stderr

            if result.returncode == 134:
                symptoms.append('abort')
            elif result.returncode == 139:
                symptoms.append('segfault')
            elif result.returncode == -1:
                symptoms.append('timeout')

            if 'corrupted' in combined or 'double free' in combined:
                symptoms.append('memory_corruption')

            if 'FAIL' in combined and result.returncode == 0:
                symptoms.append('test_failure')

            if 'count' in combined.lower() and 'expected' in combined.lower():
                symptoms.append('count_mismatch')

            print(f"Detected symptoms: {symptoms}")
            return symptoms

        except subprocess.TimeoutExpired:
            print("Detected symptom: timeout/hang")
            return ['timeout']

    def run_auto_mode(self):
        """Automatically select tools based on symptoms"""
        symptoms = self.detect_symptoms()

        # Choose tools based on symptoms
        tools_to_run = []

        if 'memory_corruption' in symptoms or 'abort' in symptoms:
            print("\n=== Memory corruption detected, running Valgrind ===")
            tools_to_run.append(('valgrind', {'tool': 'memcheck'}))

        if 'timeout' in symptoms:
            print("\n=== Timeout detected, checking for deadlock ===")
            tools_to_run.append(('valgrind', {'tool': 'helgrind'}))
            tools_to_run.append(('gdb', {}))

        if 'segfault' in symptoms:
            print("\n=== Segfault detected, running GDB ===")
            tools_to_run.append(('gdb', {}))

        if 'test_failure' in symptoms or 'count_mismatch' in symptoms:
            print("\n=== Test failure detected, recording with rr ===")
            tools_to_run.append(('rr', {}))

        # If no specific symptoms, run basic suite
        if not tools_to_run:
            print("\n=== No specific symptoms, running basic suite ===")
            tools_to_run = [
                ('valgrind', {'tool': 'memcheck'}),
                ('gdb', {})
            ]

        return tools_to_run

    def run(self):
        """Run the debugging suite"""
        print(f"\n{'='*70}")
        print(f"NIMCP Debug Suite - Code Surgeon")
        print(f"{'='*70}")
        print(f"Test: {self.test_binary}")
        print(f"Filter: {self.test_filter or 'ALL'}")
        print(f"Mode: {self.mode}")
        print(f"{'='*70}\n")

        test_cmd = self.build_test_command()

        # Determine which tools to run
        if self.mode == 'auto':
            tools_to_run = self.run_auto_mode()
        elif self.mode == 'quick':
            tools_to_run = [('gdb', {})]
        elif self.mode == 'memory':
            tools_to_run = [
                ('valgrind', {'tool': 'memcheck'}),
                ('sanitizer', {'sanitizer': 'address'})
            ]
        elif self.mode == 'threading':
            tools_to_run = [
                ('valgrind', {'tool': 'helgrind'}),
                ('valgrind', {'tool': 'drd'}),
                ('sanitizer', {'sanitizer': 'thread'})
            ]
        elif self.mode == 'crash':
            tools_to_run = [('gdb', {})]
        elif self.mode == 'record':
            tools_to_run = [('rr', {})]
        elif self.mode == 'fuzz':
            tools_to_run = [('fuzz', {})]
        else:
            tools_to_run = [('gdb', {})]

        # Run each tool
        for tool_name, kwargs in tools_to_run:
            print(f"\n{'='*70}")
            print(f"Running: {tool_name}")
            print(f"{'='*70}")

            if tool_name == 'valgrind':
                tool = ValgrindTool(self.session)
            elif tool_name == 'gdb':
                tool = GDBTool(self.session)
            elif tool_name == 'rr':
                tool = RRTool(self.session)
            elif tool_name == 'sanitizer':
                tool = SanitizerTool(self.session, kwargs.get('sanitizer', 'address'))
            elif tool_name == 'fuzz':
                fuzz_target = getattr(self, 'fuzz_target', self.test_binary)
                tool = FuzzTool(self.session, fuzz_target=fuzz_target)
                # Pass fuzz_duration to the tool run method
                kwargs['duration'] = getattr(self, 'fuzz_duration', 300)
            else:
                continue

            if not tool.is_available():
                print(f"⚠ {tool_name} not available, skipping")
                continue

            result = tool.run(test_cmd, **kwargs)
            self.session.add_result(result)

            # Print immediate findings
            if result.issues_found:
                print(f"\n🔍 Issues found ({len(result.issues_found)}):")
                for issue in result.issues_found[:5]:  # Show first 5
                    print(f"  - {issue}")

            if result.recommendations:
                print(f"\n💡 Recommendations:")
                for rec in result.recommendations[:5]:  # Show first 5
                    print(f"  - {rec}")

        # Generate report
        report_file = self.session.save(self.report_dir)

        print(f"\n{'='*70}")
        print(f"Debug Session Complete")
        print(f"{'='*70}")
        print(f"Report saved to: {report_file}")

        # Print summary
        print(f"\n📊 Summary:")
        print(f"  Tools run: {len(self.session.results)}")
        total_issues = sum(len(r.issues_found) for r in self.session.results)
        print(f"  Total issues: {total_issues}")

        # Print all recommendations
        all_recs = []
        for result in self.session.results:
            all_recs.extend(result.recommendations)

        if all_recs:
            print(f"\n🎯 All Recommendations:")
            for i, rec in enumerate(all_recs, 1):
                print(f"  {i}. {rec}")

        return self.session


def main():
    parser = argparse.ArgumentParser(
        description='NIMCP Debugging Suite - Code Surgeon',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )

    parser.add_argument('--test', required=True, help='Test binary to debug')
    parser.add_argument('--filter', help='GTest filter')
    parser.add_argument('--mode', default='auto',
                       choices=['auto', 'quick', 'memory', 'threading', 'crash', 'record', 'fuzz', 'interactive'],
                       help='Debugging mode (default: auto)')
    parser.add_argument('--breakpoints', nargs='+', help='Breakpoints for GDB mode')
    parser.add_argument('--fuzz-target', help='Path to fuzzer binary (for fuzz mode)')
    parser.add_argument('--fuzz-duration', type=int, default=300, help='Fuzzing duration in seconds (default: 300)')

    args = parser.parse_args()

    suite = DebugSuite(args.test, args.filter, args.mode)

    # Set fuzzing parameters if provided
    if args.fuzz_target:
        suite.fuzz_target = args.fuzz_target
    if args.fuzz_duration:
        suite.fuzz_duration = args.fuzz_duration

    suite.run()


if __name__ == '__main__':
    main()
