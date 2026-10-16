#!/usr/bin/env python3
"""
Auto Fixer - Claude-Integrated Automatic Bug Fixing

WHAT: Automatically fix test failures using AI assistance
WHY:  Eliminate manual debugging, self-healing tests
HOW:  Send context to Claude, apply fixes, verify

PATTERNS: Strategy, Command, Functional
"""

from dataclasses import dataclass
from typing import Tuple, Optional
from pathlib import Path
import subprocess
import json

#==============================================================================
# Data Structures
#==============================================================================

@dataclass(frozen=True)
class Fix:
    """
    Immutable fix representation

    WHAT: Code change to fix a failure
    WHY:  Track fix history, enable rollback
    HOW:  Frozen dataclass with file and changes
    """
    file_path: str
    old_code: str
    new_code: str
    explanation: str
    confidence: float  # 0.0 to 1.0
    line_number: Optional[int] = None

@dataclass(frozen=True)
class FixResult:
    """
    Immutable fix application result

    WHAT: Outcome of applying a fix
    WHY:  Track success/failure of fixes
    HOW:  Frozen dataclass
    """
    success: bool
    fix: Fix
    error_message: Optional[str] = None
    test_passed: bool = False

#==============================================================================
# Claude Integration (Pure Interface)
#==============================================================================

def prepare_fix_context(test_name: str,
                        failure_analysis: dict,
                        debug_output: str,
                        source_files: Tuple[str, ...]) -> str:
    """
    Prepare context for Claude

    WHAT: Format all context into prompt
    WHY:  Give Claude complete information
    HOW:  Structured markdown format

    COMPLEXITY: O(n) where n = source file size
    """
    context = f"""# Fix Request for Test Failure

## Test Information
- Test Name: {test_name}
- Failure Type: {failure_analysis.get('failure_type', 'unknown')}
- Error Message: {failure_analysis.get('error_message', 'none')}

## Debug Output
```
{debug_output}
```

## Relevant Source Files
"""

    for file_content in source_files:
        context += f"\n{file_content}\n"

    context += """

## Task
Please analyze this test failure and provide:
1. Root cause explanation
2. Specific code fix (with file path and line numbers)
3. Why this fix resolves the issue

Format your response as:
FILE: <file_path>
LINE: <line_number>
OLD CODE:
```
<old code>
```
NEW CODE:
```
<new code>
```
EXPLANATION: <why this fixes it>
CONFIDENCE: <0.0 to 1.0>
"""

    return context

def request_fix_from_claude(context: str) -> str:
    """
    Request fix from Claude

    WHAT: Send context to Claude, get fix
    WHY:  AI-powered automatic fixing
    HOW:  Since you ARE Claude, this is a no-op

    NOTE: In actual implementation, this would call Claude API
          But since the user IS using Claude (me), this returns
          a placeholder for the orchestration to work with the
          human (Claude) in the conversation.
    """
    return context  # Return context for human (Claude) to process

def parse_claude_fix(response: str) -> Optional[Fix]:
    """
    Parse Claude's fix response

    WHAT: Extract fix from Claude response
    WHY:  Convert text to structured Fix object
    HOW:  Parse markdown-formatted response

    COMPLEXITY: O(n) where n = response length
    """
    # Guard clause
    if not response:
        return None

    # Simple parser for now - will be enhanced
    lines = response.split('\n')

    file_path = None
    line_number = None
    old_code = []
    new_code = []
    explanation = ""
    confidence = 0.8

    in_old = False
    in_new = False

    for line in lines:
        if line.startswith('FILE:'):
            file_path = line.split(':', 1)[1].strip()
        elif line.startswith('LINE:'):
            line_number = int(line.split(':', 1)[1].strip())
        elif line.startswith('OLD CODE:'):
            in_old = True
            in_new = False
        elif line.startswith('NEW CODE:'):
            in_old = False
            in_new = True
        elif line.startswith('EXPLANATION:'):
            explanation = line.split(':', 1)[1].strip()
            in_old = False
            in_new = False
        elif line.startswith('CONFIDENCE:'):
            try:
                confidence = float(line.split(':', 1)[1].strip())
            except:
                confidence = 0.8
        elif in_old and line.strip() and not line.startswith('```'):
            old_code.append(line)
        elif in_new and line.strip() and not line.startswith('```'):
            new_code.append(line)

    if not file_path:
        return None

    return Fix(
        file_path=file_path,
        old_code='\n'.join(old_code),
        new_code='\n'.join(new_code),
        explanation=explanation,
        confidence=confidence,
        line_number=line_number
    )

#==============================================================================
# Fix Application
#==============================================================================

def apply_fix(fix: Fix, nimcp_root: Path) -> FixResult:
    """
    Apply fix to source file

    WHAT: Write fix to disk
    WHY:  Implement the proposed change
    HOW:  File I/O with backup

    NOTE: Side effect - modifies files
    """
    # Guard clause
    if not fix.file_path:
        return FixResult(
            success=False,
            fix=fix,
            error_message="No file path specified"
        )

    file_path = nimcp_root / fix.file_path

    # Guard: file must exist
    if not file_path.exists():
        return FixResult(
            success=False,
            fix=fix,
            error_message=f"File not found: {file_path}"
        )

    try:
        # Read current content
        content = file_path.read_text()

        # Apply fix (simple string replacement for now)
        if fix.old_code in content:
            new_content = content.replace(fix.old_code, fix.new_code, 1)

            # Backup original
            backup_path = file_path.with_suffix('.bak')
            file_path.rename(backup_path)

            # Write new content
            file_path.write_text(new_content)

            return FixResult(
                success=True,
                fix=fix,
                error_message=None
            )
        else:
            return FixResult(
                success=False,
                fix=fix,
                error_message="Old code not found in file"
            )

    except Exception as e:
        return FixResult(
            success=False,
            fix=fix,
            error_message=str(e)
        )

def verify_fix(fix_result: FixResult,
               test_binary: Path,
               timeout_sec: int = 300) -> FixResult:
    """
    Verify fix by running test

    WHAT: Rerun test after fix
    WHY:  Confirm fix resolved issue
    HOW:  Execute test, check return code

    NOTE: Side effect - runs test
    """
    # Guard clause
    if not fix_result.success:
        return fix_result

    try:
        result = subprocess.run(
            [str(test_binary)],
            capture_output=True,
            timeout=timeout_sec,
            cwd=test_binary.parent
        )

        test_passed = (result.returncode == 0)

        return FixResult(
            success=fix_result.success,
            fix=fix_result.fix,
            error_message=fix_result.error_message,
            test_passed=test_passed
        )

    except Exception as e:
        return FixResult(
            success=fix_result.success,
            fix=fix_result.fix,
            error_message=fix_result.error_message,
            test_passed=False
        )
