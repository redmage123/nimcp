#!/usr/bin/env python3
"""
Test Runner - Parallel Test Execution

WHAT: Execute tests in parallel with proper resource management
WHY:  Maximize throughput, minimize test time
HOW:  ProcessPoolExecutor with configurable parallelism

PATTERNS: Worker Pool, Future/Promise, Functional
"""

import subprocess
from pathlib import Path
from typing import Tuple, Optional
from dataclasses import dataclass
from concurrent.futures import ProcessPoolExecutor, as_completed
from datetime import datetime
import multiprocessing

#==============================================================================
# Pure Functions
#==============================================================================

def get_optimal_worker_count() -> int:
    """
    Calculate optimal worker count

    WHAT: Determine CPU-based parallelism
    WHY:  Balance load vs overhead
    HOW:  CPU count with cap at 16

    COMPLEXITY: O(1)
    """
    cpu_count = multiprocessing.cpu_count()
    # Cap at 16 to avoid excessive overhead
    return min(cpu_count, 16)

def execute_test_binary(binary_path: Path,
                        timeout_sec: int = 300) -> dict:
    """
    Execute single test (worker function)

    WHAT: Run test binary and capture result
    WHY:  Worker function for parallel execution
    HOW:  subprocess with timeout

    NOTE: Must be picklable for multiprocessing
    """
    # Guard clauses
    if not binary_path.exists():
        return {
            'name': binary_path.name,
            'status': 'skip',
            'duration_ms': 0.0,
            'output': '',
            'error': 'Binary not found'
        }

    start_time = datetime.now()

    try:
        # Configure ASan environment
        # WHY: Disable leak detection for tests (many have intentional leaks in test fixtures)
        # WHY: Preload ASan to avoid runtime ordering issues
        import os
        env = os.environ.copy()
        env['ASAN_OPTIONS'] = 'detect_leaks=0:detect_odr_violation=0'

        # Find and preload libasan
        try:
            libasan = subprocess.check_output(['gcc', '-print-file-name=libasan.so'],
                                             text=True, stderr=subprocess.DEVNULL).strip()
            if libasan and libasan != 'libasan.so':
                env['LD_PRELOAD'] = libasan
            else:
                # Fallback: try common locations
                import os.path
                for path in ['/usr/lib/gcc/x86_64-linux-gnu/13/libasan.so',
                            '/usr/lib/x86_64-linux-gnu/libasan.so.8']:
                    if os.path.exists(path):
                        env['LD_PRELOAD'] = path
                        break
        except Exception:
            # Fallback: try common locations
            import os.path
            for path in ['/usr/lib/gcc/x86_64-linux-gnu/13/libasan.so',
                        '/usr/lib/x86_64-linux-gnu/libasan.so.8']:
                if os.path.exists(path):
                    env['LD_PRELOAD'] = path
                    break

        result = subprocess.run(
            [str(binary_path)],
            capture_output=True,
            text=True,
            timeout=timeout_sec,
            cwd=binary_path.parent,
            env=env
        )

        duration = (datetime.now() - start_time).total_seconds() * 1000
        status = 'pass' if result.returncode == 0 else 'fail'

        return {
            'name': binary_path.name,
            'path': str(binary_path),
            'status': status,
            'duration_ms': duration,
            'output': result.stdout,
            'error': result.stderr if result.returncode != 0 else None,
            'returncode': result.returncode
        }

    except subprocess.TimeoutExpired:
        return {
            'name': binary_path.name,
            'path': str(binary_path),
            'status': 'timeout',
            'duration_ms': timeout_sec * 1000,
            'output': '',
            'error': f'Timeout after {timeout_sec}s'
        }

    except Exception as e:
        return {
            'name': binary_path.name,
            'path': str(binary_path),
            'status': 'crash',
            'duration_ms': 0.0,
            'output': '',
            'error': str(e)
        }

def run_tests_parallel(binaries: Tuple[Path, ...],
                       timeout_sec: int = 300,
                       max_workers: Optional[int] = None) -> Tuple[dict, ...]:
    """
    Execute tests in parallel

    WHAT: Run all tests concurrently
    WHY:  Minimize total execution time
    HOW:  ProcessPoolExecutor

    COMPLEXITY: O(longest_test) with parallelism
    """
    if not binaries:
        return tuple()

    if max_workers is None:
        max_workers = get_optimal_worker_count()

    results = []

    with ProcessPoolExecutor(max_workers=max_workers) as executor:
        # Submit all tests
        future_to_binary = {
            executor.submit(execute_test_binary, binary, timeout_sec): binary
            for binary in binaries
        }

        # Collect results as they complete
        for future in as_completed(future_to_binary, timeout=timeout_sec + 30):
            binary = future_to_binary[future]
            try:
                result = future.result(timeout=10)  # Timeout on getting result
                results.append(result)
            except Exception as e:
                # Worker crashed or timed out
                results.append({
                    'name': binary.name,
                    'path': str(binary),
                    'status': 'crash',
                    'duration_ms': 0.0,
                    'output': '',
                    'error': f'Worker exception: {str(e)}'
                })

    return tuple(results)
