#!/usr/bin/env python3
"""
Replace bridge boilerplate in src/cognitive/ files with macro calls.

Strategy: Use regex to find and replace the boilerplate pattern.
The pattern consists of:
1. health_agent_macros.h include (replaced with bridge_boilerplate.h)
2. mesh_participant.h + mesh_adapter.h includes (kept)
3. NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(module_name) (removed - now in macro)
4. Mesh registration block (~25 lines) (removed - now in macro)
5. Optional heartbeat_instance function (~10 lines) (removed - now in macro)

The replacement keeps the includes but swaps out health_agent_macros.h for
bridge_boilerplate.h, and replaces the boilerplate code with a single macro call.
"""

import os
import re
import sys

COGNITIVE_DIR = "/home/bbrelin/nimcp/src/cognitive"


def find_files_with_mesh_registration(directory):
    """Find all .c files with mesh_participant_register and DECLARE_HEALTH_AGENT_ATOMIC."""
    result = []
    for root, dirs, files in os.walk(directory):
        for f in files:
            if f.endswith('.c'):
                path = os.path.join(root, f)
                try:
                    with open(path, 'r') as fh:
                        content = fh.read()
                    if ('mesh_participant_register(' in content and
                        'NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(' in content and
                        'BRIDGE_BOILERPLATE' not in content):  # Skip already-processed
                        result.append(path)
                except:
                    pass
    return sorted(result)


def process_file(path, dry_run=False):
    """Process a single file, replacing boilerplate with macro."""
    with open(path, 'r') as f:
        content = f.read()

    # Extract module name
    m = re.search(r'NIMCP_DECLARE_HEALTH_AGENT_ATOMIC\((\w+)\)', content)
    if not m:
        return None, "No NIMCP_DECLARE_HEALTH_AGENT_ATOMIC found"
    module_name = m.group(1)

    # Extract category
    m = re.search(r'mesh_adapter_get_default_channel\((MESH_ADAPTER_CATEGORY_\w+)\)', content)
    if not m:
        return None, "No MESH_ADAPTER_CATEGORY found"
    category = m.group(1)

    # Detect heartbeat_instance
    has_heartbeat = f'{module_name}_heartbeat_instance(' in content

    # Build regex pattern to match the entire boilerplate block
    # Match from health_agent_macros.h include through mesh_unregister (and optionally heartbeat)

    # Step 1: Replace health_agent_macros.h include with bridge_boilerplate.h
    content = content.replace(
        '#include "utils/fault_tolerance/nimcp_health_agent_macros.h"',
        '#include "utils/bridge/nimcp_bridge_boilerplate.h"'
    )

    # Step 2: Build regex to match DECLARE + mesh registration + optional heartbeat
    # The DECLARE line
    declare_pattern = re.escape(f'NIMCP_DECLARE_HEALTH_AGENT_ATOMIC({module_name})')

    # Mesh registration block pattern (flexible whitespace)
    mesh_reg_pattern = (
        r'//=+\s*\n'                          # separator
        r'// Mesh Participant Registration\s*\n'  # header
        r'//=+\s*\n'                          # separator
        r'\s*'                                  # blank lines
        r'static mesh_participant_id_t g_' + re.escape(module_name) + r'_mesh_id = 0;\s*\n'
        r'static mesh_participant_registry_t\* g_' + re.escape(module_name) + r'_mesh_registry = NULL;\s*\n'
        r'\s*'                                  # blank line
        r'nimcp_error_t ' + re.escape(module_name) + r'_mesh_register\(mesh_participant_registry_t\* registry\)\s*\{[^}]*\}\s*\n'
        r'\s*'
        r'void ' + re.escape(module_name) + r'_mesh_unregister\(void\)\s*\{[^}]*\}'
    )

    # Heartbeat instance pattern (flexible)
    heartbeat_pattern = (
        r'\s*\n'                              # blank lines
        r'(?:/\*\*[^*]*\*\*?/\s*\n)?'        # optional doxygen comment
        r'(?:\/\*[^*]*\*\/\s*\n)?'           # optional block comment
        r'static\s+inline\s+void\s+' + re.escape(module_name) + r'_heartbeat_instance\s*\([^)]*\)\s*\{[^}]*\}'
    )

    # Also handle the "Phase 8" variant with instance_health_agent variable
    instance_agent_pattern = (
        r'\s*\n'
        r'(?:/\* =+[^*]*=+ \*/\s*\n'        # section header
        r'(?://[^\n]*\n)*)?'                   # optional comment lines
        r'(?:static\s+nimcp_health_agent_t\*\s+g_' + re.escape(module_name) + r'_instance_health_agent\s*=\s*NULL;\s*\n)?'
    )

    # Build the full replacement regex
    if has_heartbeat:
        # Try to match DECLARE + mesh + heartbeat
        full_pattern = (
            declare_pattern + r'\s*\n'
            + mesh_reg_pattern
            + r'\s*\n'  # blank lines between mesh and heartbeat
            + r'(?:'    # optional Phase 8 header/instance var
            + r'(?:/\*\s*=+[^*]*=+\s*\*/\s*\n)'  # section header
            + r'(?:[^\n]*\n)*?'  # any lines until instance var
            + r'(?:static\s+nimcp_health_agent_t\*\s+g_\w+_instance_health_agent\s*=\s*NULL;\s*\n)'
            + r'\s*'
            + r')?'     # end optional
            + r'(?:/\*\*[^\n]*\*/\s*\n)?'  # optional doxygen comment
            + r'static\s+inline\s+void\s+' + re.escape(module_name) + r'_heartbeat_instance\s*\([^)]*\)\s*\{[^}]*\}'
        )
        macro_call = f'BRIDGE_BOILERPLATE({module_name}, {category})'
    else:
        # Just DECLARE + mesh
        full_pattern = (
            declare_pattern + r'\s*\n'
            + mesh_reg_pattern
        )
        macro_call = f'BRIDGE_BOILERPLATE_MESH_ONLY({module_name}, {category})'

    result = re.subn(full_pattern, macro_call, content)
    if result[1] == 0:
        # Try a simpler approach - just match the blocks individually
        return process_file_simple(path, content, module_name, category, has_heartbeat, dry_run)

    new_content = result[0]

    if not dry_run:
        with open(path, 'w') as f:
            f.write(new_content)

    macro_type = "BRIDGE_BOILERPLATE" if has_heartbeat else "BRIDGE_BOILERPLATE_MESH_ONLY"
    return macro_type, f"{module_name}, {category}"


def process_file_simple(path, content, module_name, category, has_heartbeat, dry_run):
    """Simpler line-based approach when regex doesn't match."""
    lines = content.split('\n')

    # Find key line indices
    declare_idx = None
    mesh_unreg_end = None
    heartbeat_end = None

    for i, line in enumerate(lines):
        if f'NIMCP_DECLARE_HEALTH_AGENT_ATOMIC({module_name})' in line:
            declare_idx = i

    if declare_idx is None:
        return None, "Could not find DECLARE line"

    # Find mesh_unregister closing brace
    for i in range(declare_idx, min(len(lines), declare_idx + 50)):
        if f'{module_name}_mesh_unregister' in lines[i] and 'void' in lines[i]:
            # Find closing brace
            brace_depth = 0
            for j in range(i, min(len(lines), i + 15)):
                brace_depth += lines[j].count('{') - lines[j].count('}')
                if brace_depth == 0 and '{' in ''.join(lines[i:j+1]):
                    mesh_unreg_end = j
                    break
            break

    if mesh_unreg_end is None:
        return None, "Could not find mesh_unregister end"

    # Find heartbeat_instance end if applicable
    boilerplate_end = mesh_unreg_end
    if has_heartbeat:
        for i in range(mesh_unreg_end + 1, min(len(lines), mesh_unreg_end + 25)):
            if f'{module_name}_heartbeat_instance(' in lines[i]:
                brace_depth = 0
                found_open = False
                for j in range(i, min(len(lines), i + 20)):
                    if '{' in lines[j]:
                        brace_depth += lines[j].count('{')
                        found_open = True
                    if '}' in lines[j]:
                        brace_depth -= lines[j].count('}')
                    if found_open and brace_depth == 0:
                        boilerplate_end = j
                        break
                break
        # Also capture blank lines/comments before heartbeat
        # And the instance_health_agent variable if present
        for i in range(mesh_unreg_end + 1, boilerplate_end):
            if f'g_{module_name}_instance_health_agent' in lines[i]:
                pass  # already in range

    # Consume trailing blank lines
    while boilerplate_end + 1 < len(lines) and lines[boilerplate_end + 1].strip() == '':
        boilerplate_end += 1

    # Build macro call
    if has_heartbeat:
        macro_call = f'BRIDGE_BOILERPLATE({module_name}, {category})'
    else:
        macro_call = f'BRIDGE_BOILERPLATE_MESH_ONLY({module_name}, {category})'

    # Replace from DECLARE to boilerplate_end
    new_lines = lines[:declare_idx] + [macro_call] + lines[boilerplate_end + 1:]
    new_content = '\n'.join(new_lines)

    if not dry_run:
        with open(path, 'w') as f:
            f.write(new_content)

    macro_type = "BRIDGE_BOILERPLATE" if has_heartbeat else "BRIDGE_BOILERPLATE_MESH_ONLY"
    return macro_type, f"{module_name}, {category}"


def main():
    dry_run = '--dry-run' in sys.argv
    verbose = '--verbose' in sys.argv or '-v' in sys.argv
    single_file = None
    for arg in sys.argv[1:]:
        if not arg.startswith('-') and arg.endswith('.c'):
            single_file = arg

    if single_file:
        files = [single_file]
    else:
        files = find_files_with_mesh_registration(COGNITIVE_DIR)

    print(f"Found {len(files)} files to process")

    success_count = 0
    skip_count = 0
    skipped = []

    for path in files:
        relpath = os.path.relpath(path, '/home/bbrelin/nimcp')
        result, info = process_file(path, dry_run=dry_run)
        if result is None:
            if verbose:
                print(f"  SKIP {relpath}: {info}")
            skip_count += 1
            skipped.append((relpath, info))
        else:
            if verbose:
                print(f"  OK   {relpath}: {result}({info})")
            success_count += 1

    print(f"\nResults: {success_count} replaced, {skip_count} skipped")
    if skipped and verbose:
        print("\nSkipped files:")
        for path, reason in skipped:
            print(f"  {path}: {reason}")
    if dry_run:
        print("(DRY RUN - no files modified)")


if __name__ == '__main__':
    main()
