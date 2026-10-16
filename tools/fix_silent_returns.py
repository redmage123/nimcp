#!/usr/bin/env python3
"""
fix_silent_returns.py - Automated transformation of silent return failures
to NIMCP_THROW_TO_IMMUNE exception handling.

Transforms patterns like:
    if (!ptr) return -1;
Into:
    if (!ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "func_name: ptr is NULL");
        return -1;
    }

And multi-line patterns like:
    if (!alloc) {
        return NULL;
    }
Into:
    if (!alloc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "func_name: allocation failed");
        return NULL;
    }
"""

import re
import os
import sys
from collections import defaultdict

# Files to NEVER modify (infinite recursion risk or pre-main execution)
EXCLUDED_FILES = {
    'nimcp_memory.c',
    'nimcp_unified_memory.c',
    'nimcp_constant_time.c',
}

# Directories to skip entirely
EXCLUDED_DIRS = {'test', 'build', 'examples', '.git'}

# The include we need to ensure exists
REQUIRED_INCLUDE = '#include "utils/exception/nimcp_exception_macros.h"'

# Error code mapping based on condition patterns
def determine_error_code(condition, return_val, context_lines):
    """Determine the appropriate NIMCP error code based on the condition."""
    cond = condition or ''
    cond_lower = cond.lower()
    ctx = ' '.join(context_lines).lower()

    # Memory allocation failures (check FIRST - highest priority)
    if 'alloc' in ctx or 'calloc' in ctx or 'malloc' in ctx or 'realloc' in ctx:
        if return_val in ('NULL', '-1', 'false', 'FALSE'):
            # Check if the condition is about a recently allocated pointer
            if re.search(r'!\s*\w+', cond):
                return 'NIMCP_ERROR_NO_MEMORY'
    if 'alloc' in cond_lower or 'memory' in cond_lower or 'no_mem' in cond_lower:
        return 'NIMCP_ERROR_NO_MEMORY'
    if return_val == 'NULL' and ('create' in ctx or 'new' in ctx):
        return 'NIMCP_ERROR_NO_MEMORY'

    # NULL pointer checks (simple ! checks)
    if re.search(r'!\s*\w+(\s*\|\|\s*!\s*\w+)*\s*$', cond):
        return 'NIMCP_ERROR_NULL_POINTER'
    if re.search(r'!\s*\w+(\s*\|\|\s*!\s*\w+)+', cond):
        return 'NIMCP_ERROR_NULL_POINTER'
    # Single null check like !ptr
    if re.match(r'^!\s*\w+$', cond.strip()):
        return 'NIMCP_ERROR_NULL_POINTER'
    # Member access null check like !ptr->member
    if re.search(r'!\s*\w+->\w+', cond):
        return 'NIMCP_ERROR_NULL_POINTER'

    # Initialization checks
    if 'init' in cond_lower or 'initialized' in cond_lower:
        return 'NIMCP_ERROR_NOT_INITIALIZED'

    # Capacity/full checks
    if '>=' in cond and ('max' in cond_lower or 'capacity' in cond_lower or 'count' in cond_lower):
        return 'NIMCP_ERROR_BUFFER_OVERFLOW'

    # Size/dimension validation
    if '== 0' in cond:
        return 'NIMCP_ERROR_INVALID_PARAM'
    if 'size' in cond_lower or 'dim' in cond_lower or 'len' in cond_lower:
        return 'NIMCP_ERROR_INVALID_PARAM'

    # Range/bounds checks
    if 'index' in cond_lower or 'bound' in cond_lower or 'range' in cond_lower:
        return 'NIMCP_ERROR_OUT_OF_RANGE'

    # Mutex/lock failures (only if the condition itself mentions mutex)
    if 'mutex' in cond_lower or 'lock' in cond_lower:
        return 'NIMCP_ERROR_OPERATION_FAILED'

    # Default based on return value
    if return_val == 'NULL':
        return 'NIMCP_ERROR_NULL_POINTER'
    if return_val in ('-1', 'false', 'FALSE'):
        return 'NIMCP_ERROR_INVALID_PARAM'

    return 'NIMCP_ERROR_INVALID_PARAM'


def extract_condition_description(condition):
    """Generate a human-readable description from the condition."""
    if not condition:
        return 'operation failed'

    # Multi-parameter null checks: !a || !b || !c
    null_checks = re.findall(r'!\s*([\w>-]+)', condition)
    if null_checks and all(re.match(r'^[\w>-]+$', n) for n in null_checks):
        if len(null_checks) == 1:
            var = null_checks[0]
            if '->' in var:
                return f'{var} is NULL'
            return f'{var} is NULL'
        return f'required parameter is NULL ({", ".join(null_checks)})'

    # Size zero checks
    if '== 0' in condition:
        var = re.search(r'([\w.>-]+)\s*==\s*0', condition)
        if var:
            return f'{var.group(1)} is zero'

    # Capacity checks
    if '>=' in condition:
        return 'capacity exceeded'

    # Comparison checks
    if '!=' in condition or '==' in condition or '<' in condition or '>' in condition:
        return 'validation failed'

    return 'validation failed'


def find_current_function(lines, line_idx):
    """Find the function name for the given line index by searching backwards.

    Uses a simple heuristic: scan backwards for lines at column 0 that look
    like function definitions (type name( pattern followed by {).
    """
    keywords = {'if', 'while', 'for', 'switch', 'return', 'sizeof', 'typeof',
                'else', 'do', 'case', 'defined', 'pragma', 'struct', 'union',
                'enum', 'typedef', 'extern', 'static', 'inline', 'const',
                'volatile', 'register', 'void', 'int', 'char', 'float',
                'double', 'long', 'short', 'unsigned', 'signed', 'bool',
                'uint32_t', 'uint64_t', 'int32_t', 'int64_t', 'size_t',
                'ssize_t', 'uint8_t', 'int8_t', 'uint16_t', 'int16_t',
                'nimcp_error_t', 'true', 'false'}

    for i in range(line_idx, max(-1, line_idx - 200), -1):
        line = lines[i]
        # Look for function-level opening brace at start of line or after )
        stripped = line.rstrip()

        # Pattern 1: function_name(...) {   or  ) {  at low indent
        # Pattern 2: line with just { at column 0
        is_func_brace = False
        if stripped == '{' or stripped.endswith(') {') or stripped.endswith('){'):
            indent = len(line) - len(line.lstrip())
            if indent <= 0:
                is_func_brace = True

        if not is_func_brace:
            continue

        # Search this line and preceding lines for the function name
        for j in range(i, max(-1, i - 15), -1):
            l = lines[j]
            # Find word( pattern - the function name
            matches = re.findall(r'(\w+)\s*\(', l)
            for match in matches:
                if match not in keywords:
                    return match
        break

    return 'unknown'


def is_already_handled(lines, line_idx):
    """Check if the return already has a NIMCP_THROW nearby."""
    start = max(0, line_idx - 5)
    for i in range(start, line_idx):
        if 'NIMCP_THROW' in lines[i] or 'NIMCP_CHECK_THROW' in lines[i] or 'NIMCP_API_CHECK' in lines[i]:
            return True
    return False


def is_normal_flow(lines, line_idx, stripped):
    """Check if this return is normal flow (not an error path)."""
    # "return false;" at the end of a function that returns bool for a search
    # is normal flow. We detect this by checking if it's the last statement
    # before a closing brace with no if-guard.

    # Check if preceded by a closing loop/if or is a fall-through
    if line_idx > 0:
        prev = lines[line_idx - 1].strip()
        # If previous line is just "}" this is likely a fall-through at function end
        if prev == '}' and stripped in ('return false;', 'return FALSE;'):
            return True

    # Return false at the end of a series of if-checks (fall-through default)
    # Look for pattern: ends with "return false;" preceded by "}" from if blocks
    if stripped in ('return false;', 'return FALSE;'):
        # Check if there's no direct if-guard on this line or within 2 lines before
        has_guard = False
        for i in range(max(0, line_idx - 2), line_idx):
            l = lines[i].strip()
            if l.startswith('if ') or l.startswith('if('):
                has_guard = True
                break
        if not has_guard:
            # Check if there's a } on the line before (end of previous if block)
            if line_idx > 0 and lines[line_idx - 1].strip() == '}':
                return True

    return False


def process_inline_return(line, indent, func_name, lines, line_idx):
    """
    Transform an inline return like:
        if (!ptr) return -1;
    Into:
        if (!ptr) {
            NIMCP_THROW_TO_IMMUNE(ERROR_CODE, "func: description");
            return -1;
        }
    """
    # Match: if (condition) return value;
    match = re.match(r'^(\s*)(if\s*\((.+)\))\s+return\s+(.*?)\s*;', line)
    if not match:
        return None

    orig_indent = match.group(1)
    if_clause = match.group(2)
    condition = match.group(3)
    return_val = match.group(4)

    context = [lines[j] for j in range(max(0, line_idx - 5), line_idx)]
    error_code = determine_error_code(condition, return_val, context)
    description = extract_condition_description(condition)

    inner_indent = orig_indent + '    '

    new_lines = [
        f'{orig_indent}{if_clause} {{\n',
        f'{inner_indent}NIMCP_THROW_TO_IMMUNE({error_code}, "{func_name}: {description}");\n',
        f'{inner_indent}return {return_val};\n',
        f'{orig_indent}}}\n',
    ]
    return new_lines


def process_block_return(lines, line_idx, func_name):
    """
    Add NIMCP_THROW_TO_IMMUNE before a standalone return in a block:
        if (!ptr) {
            return -1;    <-- add throw before this
        }
    """
    line = lines[line_idx]
    match = re.match(r'^(\s*)return\s+(.*?)\s*;', line)
    if not match:
        return None

    indent = match.group(1)
    return_val = match.group(2)

    # Get the condition from the enclosing if block
    condition = ''
    for i in range(line_idx - 1, max(-1, line_idx - 10), -1):
        l = lines[i].strip()
        if_match = re.match(r'if\s*\((.+)\)\s*\{?', l)
        if if_match:
            condition = if_match.group(1)
            break

    context = [lines[j] for j in range(max(0, line_idx - 5), line_idx)]
    error_code = determine_error_code(condition, return_val, context)
    description = extract_condition_description(condition)

    throw_line = f'{indent}NIMCP_THROW_TO_IMMUNE({error_code}, "{func_name}: {description}");\n'
    return throw_line


def ensure_include(lines, filepath):
    """Ensure the exception macros header is included."""
    has_include = False
    last_include_idx = -1

    for i, line in enumerate(lines):
        if REQUIRED_INCLUDE in line:
            has_include = True
            break
        if line.strip().startswith('#include'):
            last_include_idx = i

    if not has_include and last_include_idx >= 0:
        lines.insert(last_include_idx + 1, REQUIRED_INCLUDE + '\n')
        return True
    return False


def process_file(filepath, dry_run=False):
    """Process a single file, transforming silent returns."""
    with open(filepath, 'r') as f:
        lines = f.readlines()

    original = lines[:]
    modifications = 0
    include_added = False

    # First pass: collect all line indices that need modification
    # We process in reverse order to avoid index shifting
    modifications_list = []

    for i, line in enumerate(lines):
        stripped = line.strip()

        # Skip if already has throw nearby
        if is_already_handled(lines, i):
            continue

        # Check for inline returns: if (cond) return val;
        inline_match = re.match(r'^\s*if\s*\(.+\)\s+return\s+(-1|false|FALSE|NULL)\s*;', stripped)
        if inline_match:
            func_name = find_current_function(lines, i)
            new_lines = process_inline_return(line, '', func_name, lines, i)
            if new_lines:
                modifications_list.append(('replace', i, new_lines))
                continue

        # Check for standalone returns in blocks
        standalone_match = re.match(r'^\s*return\s+(-1|false|FALSE|NULL)\s*;', stripped)
        if standalone_match:
            return_val = standalone_match.group(1)

            # Skip normal flow returns
            if is_normal_flow(lines, i, stripped):
                continue

            func_name = find_current_function(lines, i)
            throw_line = process_block_return(lines, i, func_name)
            if throw_line:
                modifications_list.append(('insert', i, throw_line))

    if not modifications_list:
        return 0

    # Apply modifications in reverse order
    for mod_type, idx, content in reversed(modifications_list):
        if mod_type == 'replace':
            lines[idx:idx+1] = content
            modifications += 1
        elif mod_type == 'insert':
            lines.insert(idx, content)
            modifications += 1

    # Ensure include is present if we made changes
    if modifications > 0:
        include_added = ensure_include(lines, filepath)

    if not dry_run and modifications > 0:
        with open(filepath, 'w') as f:
            f.writelines(lines)

    return modifications


def main():
    import argparse
    parser = argparse.ArgumentParser(description='Fix silent return failures')
    parser.add_argument('--dry-run', action='store_true', help='Count changes without modifying files')
    parser.add_argument('--path', default='src/', help='Source directory to process')
    parser.add_argument('--file', help='Process a single file')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    args = parser.parse_args()

    total_mods = 0
    files_modified = 0
    results = {}

    if args.file:
        files_to_process = [args.file]
    else:
        files_to_process = []
        for root, dirs, files in os.walk(args.path):
            # Skip excluded directories
            dirs[:] = [d for d in dirs if d not in EXCLUDED_DIRS]
            for f in files:
                if f.endswith('.c') and f not in EXCLUDED_FILES:
                    files_to_process.append(os.path.join(root, f))

    for filepath in sorted(files_to_process):
        try:
            mods = process_file(filepath, dry_run=args.dry_run)
            if mods > 0:
                results[filepath] = mods
                total_mods += mods
                files_modified += 1
                if args.verbose:
                    print(f'  {filepath}: {mods} modifications')
        except Exception as e:
            print(f'ERROR processing {filepath}: {e}', file=sys.stderr)

    action = 'Would modify' if args.dry_run else 'Modified'
    print(f'\n{action} {total_mods} return statements across {files_modified} files')

    if args.verbose:
        print('\nTop 20 files by modifications:')
        for fp, count in sorted(results.items(), key=lambda x: -x[1])[:20]:
            print(f'  {count:4d}  {fp}')


if __name__ == '__main__':
    main()
