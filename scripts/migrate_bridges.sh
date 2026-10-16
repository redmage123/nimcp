#!/bin/bash
# Bridge migration script: Add bridge_base_t base as first struct member

INCLUDE_LINE='#include "utils/bridge/nimcp_bridge_base.h"'

migrate_file() {
    local f="$1"
    local changed=0
    
    # Skip if already migrated
    if grep -q "bridge_base_t base" "$f" 2>/dev/null; then
        return 0
    fi
    
    # Find struct definition pattern: struct xxx_bridge { or struct xxx_bridge_s { or struct xxx_bridge_struct {
    if grep -qE '^struct [a-z_]+_bridge(_s|_struct)? \{' "$f"; then
        # Add bridge_base_t base after struct opening
        sed -i '/^struct [a-z_]*_bridge\(_s\|_struct\)\? {$/a\    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */' "$f"
        changed=1
    fi
    
    # Remove standalone mutex member if present
    if grep -q '^\s*nimcp_mutex_t\* mutex;' "$f"; then
        # Remove Thread safety comment and mutex line
        sed -i '/\/\* Thread safety \*\//,+2d' "$f"
        changed=1
    fi
    
    # Add include if not present and file was changed
    if [ $changed -eq 1 ] && ! grep -q "nimcp_bridge_base.h" "$f"; then
        sed -i "0,/#include/s|#include|$INCLUDE_LINE\n#include|" "$f"
    fi
    
    if [ $changed -eq 1 ]; then
        echo "Migrated: $f"
        return 1
    fi
    return 0
}

# Process all bridge files
count=0
for f in $(find src -name "*_bridge.c" 2>/dev/null); do
    migrate_file "$f"
    if [ $? -eq 1 ]; then
        ((count++))
    fi
done

echo "Total migrated: $count"
