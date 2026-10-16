#!/usr/bin/env python3
"""
Lint Runner - Code Style and Quality Enforcement

WHAT: Run lint checks, auto-fix issues, enforce NIMCP standards
WHY:  Maintain code quality, enforce consistency
HOW:  clang-tidy + cppcheck + custom NIMCP rules

PATTERNS: Strategy, Command, Functional
"""

from dataclasses import dataclass
from typing import Tuple, Optional
from pathlib import Path
import subprocess
import re

#==============================================================================
# Data Structures
#==============================================================================

@dataclass(frozen=True)
class LintIssue:
    """
    Immutable lint issue

    WHAT: Code style/quality violation
    WHY:  Track issues for fixing
    HOW:  Frozen dataclass
    """
    file_path: str
    line_number: int
    column: int
    severity: str  # error, warning, note
    message: str
    rule: str
    auto_fixable: bool

@dataclass(frozen=True)
class LintResult:
    """
    Immutable lint result

    WHAT: Complete lint run result
    WHY:  Track all issues
    HOW:  Frozen dataclass
    """
    issues: Tuple[LintIssue, ...]
    errors: int
    warnings: int
    auto_fixed: int

#==============================================================================
# NIMCP Custom Rules (Pure Functions)
#==============================================================================

def check_function_length(file_path: Path) -> Tuple[LintIssue, ...]:
    """
    Check function length < 50 lines

    WHAT: Enforce NIMCP 50-line rule
    WHY:  Keep functions maintainable
    HOW:  Parse C/C++ functions, count lines

    COMPLEXITY: O(n) where n = file size
    """
    # Guard clause
    if not file_path.exists():
        return tuple()

    try:
        content = file_path.read_text()
        issues = []

        # Simple function length checker
        # TODO: Implement full C/C++ parser
        lines = content.split('\n')
        in_function = False
        function_start = 0
        brace_count = 0

        for i, line in enumerate(lines, 1):
            if '{' in line:
                if not in_function:
                    in_function = True
                    function_start = i
                brace_count += line.count('{')

            if '}' in line:
                brace_count -= line.count('}')
                if brace_count == 0 and in_function:
                    function_length = i - function_start + 1
                    if function_length > 50:
                        issues.append(LintIssue(
                            file_path=str(file_path),
                            line_number=function_start,
                            column=0,
                            severity='error',
                            message=f'Function exceeds 50 lines ({function_length} lines)',
                            rule='nimcp-function-length',
                            auto_fixable=False
                        ))
                    in_function = False

        return tuple(issues)

    except Exception:
        return tuple()

def check_nested_ifs(file_path: Path) -> Tuple[LintIssue, ...]:
    """
    Check for nested if statements

    WHAT: Detect nested ifs (NIMCP violation)
    WHY:  Enforce guard clause pattern
    HOW:  Track indentation levels

    COMPLEXITY: O(n) where n = file size
    """
    # Guard clause
    if not file_path.exists():
        return tuple()

    try:
        content = file_path.read_text()
        issues = []
        lines = content.split('\n')

        if_depth = 0

        for i, line in enumerate(lines, 1):
            stripped = line.strip()

            if stripped.startswith('if ') or stripped.startswith('if('):
                if_depth += 1

                if if_depth > 1:
                    issues.append(LintIssue(
                        file_path=str(file_path),
                        line_number=i,
                        column=0,
                        severity='warning',
                        message='Nested if detected - use guard clause instead',
                        rule='nimcp-no-nested-ifs',
                        auto_fixable=False
                    ))

            if '}' in stripped:
                if_depth = max(0, if_depth - 1)

        return tuple(issues)

    except Exception:
        return tuple()

def check_documentation(file_path: Path) -> Tuple[LintIssue, ...]:
    """
    Check WHAT-WHY-HOW documentation

    WHAT: Verify functions have proper docs
    WHY:  Enforce NIMCP documentation standard
    HOW:  Pattern match for WHAT/WHY/HOW

    COMPLEXITY: O(n) where n = file size
    """
    # TODO: Implement documentation checker
    return tuple()

#==============================================================================
# External Linters
#==============================================================================

def run_clang_tidy(file_paths: Tuple[Path, ...],
                   build_dir: Path,
                   auto_fix: bool = True) -> LintResult:
    """
    Run clang-tidy

    WHAT: Run clang-tidy linter
    WHY:  Catch C++ issues
    HOW:  Execute clang-tidy with NIMCP config

    NOTE: Side effect - may modify files if auto_fix=True
    """
    # Guard clause
    if not file_paths:
        return LintResult(issues=tuple(), errors=0, warnings=0, auto_fixed=0)

    issues = []

    for file_path in file_paths:
        try:
            cmd = ['clang-tidy', str(file_path), '--']

            if auto_fix:
                cmd.insert(1, '--fix')

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=build_dir
            )

            # Parse clang-tidy output
            # TODO: Implement output parser

        except Exception:
            continue

    return LintResult(
        issues=tuple(issues),
        errors=0,
        warnings=0,
        auto_fixed=0
    )

def run_cppcheck(file_paths: Tuple[Path, ...]) -> LintResult:
    """
    Run cppcheck

    WHAT: Run cppcheck static analyzer
    WHY:  Additional C/C++ checking
    HOW:  Execute cppcheck
    """
    # TODO: Implement cppcheck integration
    return LintResult(issues=tuple(), errors=0, warnings=0, auto_fixed=0)

def run_nimcp_checks(file_paths: Tuple[Path, ...]) -> LintResult:
    """
    Run NIMCP custom checks

    WHAT: Run all NIMCP-specific rules
    WHY:  Enforce NIMCP coding standards
    HOW:  Aggregate custom checkers

    COMPLEXITY: O(n * m) where n = files, m = checkers
    """
    all_issues = []

    for file_path in file_paths:
        # Run all custom checks
        all_issues.extend(check_function_length(file_path))
        all_issues.extend(check_nested_ifs(file_path))
        all_issues.extend(check_documentation(file_path))

    errors = sum(1 for i in all_issues if i.severity == 'error')
    warnings = sum(1 for i in all_issues if i.severity == 'warning')

    return LintResult(
        issues=tuple(all_issues),
        errors=errors,
        warnings=warnings,
        auto_fixed=0
    )
