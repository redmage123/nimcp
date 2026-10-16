#!/bin/bash
# Replace boilerplate in non-bridge .c files
# Usage: bash replace_nb_bp.sh

set -e

NIMCP_DIR="/home/bbrelin/nimcp"
cd "$NIMCP_DIR"

# Get all non-bridge files with boilerplate mesh_id pattern
FILES=$(grep -rl "g_.*_mesh_id = 0" src/ --include="*.c" | grep -v "_bridge.c" | sort)

SUCCESS=0
FAIL=0

for FILE in $FILES; do
    # Get module name from NIMCP_DECLARE_HEALTH_AGENT_ATOMIC
    MODULE=$(grep -oP 'NIMCP_DECLARE_HEALTH_AGENT_ATOMIC\(\K\w+' "$FILE" 2>/dev/null || true)

    # If no atomic declaration, try manual pattern
    if [ -z "$MODULE" ]; then
        MODULE=$(grep -oP 'static\s+nimcp_health_agent_t\s*\*\s*g_\K\w+(?=_health_agent\s*=\s*NULL)' "$FILE" 2>/dev/null || true)
    fi

    if [ -z "$MODULE" ]; then
        echo "SKIP: $FILE (no module name found)"
        FAIL=$((FAIL + 1))
        continue
    fi

    # Determine category from path
    CATEGORY="MESH_ADAPTER_CATEGORY_COGNITIVE"
    case "$FILE" in
        src/core/brain/subcortical/*) CATEGORY="MESH_ADAPTER_CATEGORY_SUBCORTICAL" ;;
        src/mesh/*) CATEGORY="MESH_ADAPTER_CATEGORY_SYSTEM" ;;
        src/middleware/*) CATEGORY="MESH_ADAPTER_CATEGORY_SYSTEM" ;;
        src/networking/*) CATEGORY="MESH_ADAPTER_CATEGORY_SYSTEM" ;;
        src/plasticity/*) CATEGORY="MESH_ADAPTER_CATEGORY_PLASTICITY" ;;
        src/security/*) CATEGORY="MESH_ADAPTER_CATEGORY_SECURITY" ;;
        src/glial/*) CATEGORY="MESH_ADAPTER_CATEGORY_GLIAL" ;;
    esac

    # Check for heartbeat_instance
    HAS_HEARTBEAT=0
    if grep -q "static inline void ${MODULE}_heartbeat_instance" "$FILE" 2>/dev/null; then
        HAS_HEARTBEAT=1
    fi

    # Determine macro to use
    if [ "$HAS_HEARTBEAT" -eq 1 ]; then
        MACRO="BRIDGE_BOILERPLATE(${MODULE}, ${CATEGORY})"
    else
        MACRO="BRIDGE_BOILERPLATE_MESH_ONLY(${MODULE}, ${CATEGORY})"
    fi

    # Check if already processed
    if grep -q "BRIDGE_BOILERPLATE" "$FILE" 2>/dev/null; then
        echo "SKIP: $FILE (already processed)"
        FAIL=$((FAIL + 1))
        continue
    fi

    # Use Python for the actual replacement (more reliable than sed for multi-line)
    python3 -c "
import re
import sys

filepath = '$FILE'
module = '$MODULE'
category = '$CATEGORY'
has_heartbeat = $HAS_HEARTBEAT

with open(filepath, 'r') as f:
    content = f.read()

original = content

# Replace include
content = content.replace(
    '#include \"utils/fault_tolerance/nimcp_health_agent_macros.h\"',
    '#include \"utils/bridge/nimcp_bridge_boilerplate.h\"'
)

# Choose macro
if has_heartbeat:
    macro_line = f'BRIDGE_BOILERPLATE({module}, {category})'
else:
    macro_line = f'BRIDGE_BOILERPLATE_MESH_ONLY({module}, {category})'

# Build pattern to remove: from NIMCP_DECLARE (or manual health agent) through end of boilerplate
# Pattern 1: NIMCP_DECLARE_HEALTH_AGENT_ATOMIC + mesh block + optional heartbeat
pattern = (
    r'NIMCP_DECLARE_HEALTH_AGENT_ATOMIC\(' + re.escape(module) + r'\)\s*\n'
    r'//=+\n//\s*Mesh Participant Registration\s*\n//=+\s*\n\n?'
    r'static\s+mesh_participant_id_t\s+g_' + re.escape(module) + r'_mesh_id\s*=\s*0\s*;\s*\n'
    r'static\s+mesh_participant_registry_t\s*\*\s*g_' + re.escape(module) + r'_mesh_registry\s*=\s*NULL\s*;\s*\n\n?'
    r'nimcp_error_t\s+' + re.escape(module) + r'_mesh_register\(mesh_participant_registry_t\s*\*\s*registry\)\s*\{[^}]*\}\s*\n\n?'
    r'void\s+' + re.escape(module) + r'_mesh_unregister\(void\)\s*\{[^}]*?\n\s*\}\s*\n'
)
content = re.sub(pattern, macro_line + '\n', content)

# Also try without the section header comment
pattern2 = (
    r'NIMCP_DECLARE_HEALTH_AGENT_ATOMIC\(' + re.escape(module) + r'\)\s*\n\n?'
    r'static\s+mesh_participant_id_t\s+g_' + re.escape(module) + r'_mesh_id\s*=\s*0\s*;\s*\n'
    r'static\s+mesh_participant_registry_t\s*\*\s*g_' + re.escape(module) + r'_mesh_registry\s*=\s*NULL\s*;\s*\n\n?'
    r'nimcp_error_t\s+' + re.escape(module) + r'_mesh_register\(mesh_participant_registry_t\s*\*\s*registry\)\s*\{[^}]*\}\s*\n\n?'
    r'void\s+' + re.escape(module) + r'_mesh_unregister\(void\)\s*\{[^}]*?\n\s*\}\s*\n'
)
if 'NIMCP_DECLARE_HEALTH_AGENT_ATOMIC' in content:
    content = re.sub(pattern2, macro_line + '\n', content)

# Pattern for manual health agent
if 'g_' + module + '_health_agent = NULL' in content:
    # Remove manual health agent declaration
    content = re.sub(
        r'/\*\s*Health agent:.*?\*/\s*\n',
        '',
        content
    )
    content = re.sub(
        r'static\s+nimcp_health_agent_t\s*\*\s*g_' + re.escape(module) + r'_health_agent\s*=\s*NULL\s*;\s*\n',
        '',
        content
    )
    # Remove stub heartbeat (non-instance)
    content = re.sub(
        r'/\*\s*Stub heartbeat.*?\*/\s*\n'
        r'static\s+inline\s+void\s+' + re.escape(module) + r'_heartbeat\s*\([^)]*\)\s*\{[^}]*\}\s*\n?',
        '',
        content
    )
    # Now remove mesh block
    mesh_pattern = (
        r'//=+\n//\s*Mesh Participant Registration\s*\n//=+\s*\n\n?'
        r'static\s+mesh_participant_id_t\s+g_' + re.escape(module) + r'_mesh_id\s*=\s*0\s*;\s*\n'
        r'static\s+mesh_participant_registry_t\s*\*\s*g_' + re.escape(module) + r'_mesh_registry\s*=\s*NULL\s*;\s*\n\n?'
        r'nimcp_error_t\s+' + re.escape(module) + r'_mesh_register\(mesh_participant_registry_t\s*\*\s*registry\)\s*\{[^}]*\}\s*\n\n?'
        r'void\s+' + re.escape(module) + r'_mesh_unregister\(void\)\s*\{[^}]*?\n\s*\}\s*\n'
    )
    content = re.sub(mesh_pattern, macro_line + '\n', content)

# Remove heartbeat_instance if present
if has_heartbeat:
    hb_pattern = (
        r'\n*(?:/\*\*[^*]*\*+(?:[^/*][^*]*\*+)*/\s*\n)?'
        r'(?:/\*\*\s*@brief.*?\*/\s*\n)?'
        r'static\s+inline\s+void\s+' + re.escape(module) + r'_heartbeat_instance\s*\('
        r'[^)]*\)\s*\n?\{[^}]*\}\s*\n'
    )
    content = re.sub(hb_pattern, '\n', content)

# Clean up excessive blank lines
content = re.sub(r'\n{4,}', '\n\n\n', content)

if content == original:
    print('NO_CHANGE')
    sys.exit(0)

with open(filepath, 'w') as f:
    f.write(content)
print('OK')
" 2>&1

    RESULT=$?
    if [ $RESULT -eq 0 ]; then
        SUCCESS=$((SUCCESS + 1))
        echo "OK: $FILE -> $MACRO"
    else
        FAIL=$((FAIL + 1))
        echo "FAIL: $FILE"
    fi
done

echo ""
echo "Results: $SUCCESS succeeded, $FAIL failed/skipped"
