#!/usr/bin/env python3
"""
Failure Analyzer - Intelligent Test Failure Analysis

WHAT: Categorize and analyze test failures
WHY:  Determine appropriate debugging strategy
HOW:  Pattern matching on error output + debug tool integration

PATTERNS: Strategy, Chain of Responsibility, Functional
"""

from dataclasses import dataclass
from typing import Tuple, Optional, Callable
from enum import Enum
import re

#==============================================================================
# Data Structures
#==============================================================================

class FailureType(Enum):
    """Failure categorization"""
    ASSERTION = "assertion"
    CRASH_SEGFAULT = "crash_segfault"
    CRASH_ABORT = "crash_abort"
    MEMORY_LEAK = "memory_leak"
    DOUBLE_FREE = "double_free"
    USE_AFTER_FREE = "use_after_free"
    BUFFER_OVERFLOW = "buffer_overflow"
    RACE_CONDITION = "race_condition"
    DEADLOCK = "deadlock"
    TIMEOUT = "timeout"
    UNDEFINED_BEHAVIOR = "undefined_behavior"
    UNKNOWN = "unknown"

@dataclass(frozen=True)
class FailureAnalysis:
    """
    Immutable failure analysis result

    WHAT: Complete analysis of test failure
    WHY:  Guides debugging and fixing strategy
    HOW:  Frozen dataclass with categorization
    """
    test_name: str
    failure_type: FailureType
    error_message: str
    stack_trace: Optional[str]
    recommended_tool: str  # valgrind, gdb, tsan, etc
    likely_causes: Tuple[str, ...]
    suggested_fixes: Tuple[str, ...]

#==============================================================================
# Pattern Matchers (Pure Functions)
#==============================================================================

def match_assertion_failure(output: str, error: str) -> bool:
    """
    Detect assertion failure

    WHAT: Pattern match for assertion errors
    WHY:  Most common test failure type
    HOW:  Regex for assertion keywords
    """
    patterns = [
        r'Assertion.*failed',
        r'EXPECT_.*failed',
        r'ASSERT_.*failed',
        r'Expected.*but got'
    ]
    combined = output + error
    return any(re.search(p, combined, re.IGNORECASE) for p in patterns)

def match_segfault(output: str, error: str) -> bool:
    """Detect segmentation fault"""
    patterns = [r'Segmentation fault', r'SIGSEGV', r'signal 11']
    combined = output + error
    return any(re.search(p, combined, re.IGNORECASE) for p in patterns)

def match_double_free(output: str, error: str) -> bool:
    """Detect double-free error"""
    patterns = [r'double free', r'free\(\): invalid pointer']
    combined = output + error
    return any(re.search(p, combined, re.IGNORECASE) for p in patterns)

def match_memory_leak(output: str, error: str) -> bool:
    """Detect memory leak"""
    patterns = [r'memory leak', r'leaked \d+ bytes']
    combined = output + error
    return any(re.search(p, combined, re.IGNORECASE) for p in patterns)

def match_timeout_hang(output: str, error: str) -> bool:
    """Detect timeout or hanging test"""
    patterns = [r'timeout', r'timed out', r'hang', r'no output']
    combined = output + error
    return any(re.search(p, combined, re.IGNORECASE) for p in patterns)

#==============================================================================
# Analyzer (Chain of Responsibility Pattern)
#==============================================================================

def categorize_failure(test_name: str,
                       output: str,
                       error: str,
                       returncode: int) -> FailureType:
    """
    Categorize failure type

    WHAT: Determine failure category from output
    WHY:  Enables appropriate debugging strategy
    HOW:  Chain of pattern matchers

    COMPLEXITY: O(n) where n = number of patterns
    """
    # Guard clause
    if not error and returncode == 0:
        return FailureType.UNKNOWN

    # Check patterns in priority order
    if match_double_free(output, error):
        return FailureType.DOUBLE_FREE

    if match_memory_leak(output, error):
        return FailureType.MEMORY_LEAK

    if match_segfault(output, error):
        return FailureType.CRASH_SEGFAULT

    if 'SIGABRT' in error or 'abort' in error.lower():
        return FailureType.CRASH_ABORT

    if match_assertion_failure(output, error):
        return FailureType.ASSERTION

    if returncode == 124:  # timeout signal
        return FailureType.TIMEOUT

    return FailureType.UNKNOWN

def get_recommended_tool(failure_type: FailureType) -> str:
    """
    Get debugging tool for failure type

    WHAT: Map failure type to debugging tool
    WHY:  Use right tool for the job
    HOW:  Simple mapping function
    """
    tool_map = {
        FailureType.DOUBLE_FREE: 'valgrind',
        FailureType.MEMORY_LEAK: 'valgrind',
        FailureType.USE_AFTER_FREE: 'valgrind',
        FailureType.BUFFER_OVERFLOW: 'valgrind',
        FailureType.CRASH_SEGFAULT: 'gdb',
        FailureType.CRASH_ABORT: 'gdb',
        FailureType.RACE_CONDITION: 'tsan',
        FailureType.DEADLOCK: 'helgrind',
        FailureType.UNDEFINED_BEHAVIOR: 'ubsan',
        FailureType.ASSERTION: 'gdb',
        FailureType.TIMEOUT: 'gdb',
    }
    return tool_map.get(failure_type, 'gdb')

def analyze_failure(test_result: dict) -> FailureAnalysis:
    """
    Perform complete failure analysis

    WHAT: Analyze test failure and provide recommendations
    WHY:  Guide debugging and fixing process
    HOW:  Categorize, recommend tools, suggest fixes

    COMPLEXITY: O(n) pattern matching
    """
    test_name = test_result['name']
    output = test_result.get('output', '')
    error = test_result.get('error', '')
    returncode = test_result.get('returncode', 1)

    failure_type = categorize_failure(test_name, output, error, returncode)
    recommended_tool = get_recommended_tool(failure_type)

    # Generate likely causes based on failure type
    causes = get_likely_causes(failure_type)

    # Generate suggested fixes
    fixes = get_suggested_fixes(failure_type)

    return FailureAnalysis(
        test_name=test_name,
        failure_type=failure_type,
        error_message=error or "Unknown error",
        stack_trace=None,  # Will be filled by debug tools
        recommended_tool=recommended_tool,
        likely_causes=causes,
        suggested_fixes=fixes
    )

def get_likely_causes(failure_type: FailureType) -> Tuple[str, ...]:
    """Get likely causes for failure type (pure function)"""
    causes_map = {
        FailureType.DOUBLE_FREE: (
            "Memory freed twice",
            "Incorrect reference counting",
            "Missing NULL assignment after free"
        ),
        FailureType.MEMORY_LEAK: (
            "Missing free() call",
            "Early return without cleanup",
            "Exception without cleanup"
        ),
        FailureType.CRASH_SEGFAULT: (
            "NULL pointer dereference",
            "Out of bounds access",
            "Use after free"
        ),
        FailureType.ASSERTION: (
            "Logic error",
            "Incorrect algorithm",
            "Edge case not handled"
        ),
    }
    return causes_map.get(failure_type, ("Unknown",))

def get_suggested_fixes(failure_type: FailureType) -> Tuple[str, ...]:
    """Get suggested fixes (pure function)"""
    fixes_map = {
        FailureType.DOUBLE_FREE: (
            "Set pointer to NULL after free",
            "Use reference counting",
            "Review ownership semantics"
        ),
        FailureType.MEMORY_LEAK: (
            "Add free() in all exit paths",
            "Use RAII pattern",
            "Add cleanup on exception"
        ),
        FailureType.CRASH_SEGFAULT: (
            "Add NULL checks",
            "Validate array indices",
            "Fix memory lifecycle"
        ),
    }
    return fixes_map.get(failure_type, ("Run debugger",))
